(() => {
  "use strict";

  const config = window.MESHCROSSWAY_CONFIG || {};
  const authPanel = document.getElementById("auth-panel");
  const dashboard = document.getElementById("dashboard");
  const authStatus = document.getElementById("auth-status");
  const dashboardStatus = document.getElementById("dashboard-status");
  const API = String(config.cloudApiBaseUrl || "").replace(/\/$/, "");
  let clerk;
  let messages = [];
  let favorites = [];
  let tracks = [];

  const escapeHtml = (value) => String(value ?? "").replace(/[&<>"']/g, (char) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  })[char]);
  const formatBytes = (bytes) => {
    const n = Number(bytes || 0);
    if (n < 1024) return `${n} B`;
    if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KiB`;
    return `${(n / (1024 * 1024)).toFixed(1)} MiB`;
  };
  const formatDate = (seconds) => {
    const date = new Date(Number(seconds || 0) * 1000);
    return Number.isNaN(date.getTime()) ? "" : new Intl.DateTimeFormat(undefined, { dateStyle: "medium", timeStyle: "short" }).format(date);
  };
  const networkName = (network) => network === "MESHCORE" ? "MeshCore" : "Meshtastic";
  const setStatus = (message, kind = "") => {
    dashboardStatus.textContent = message;
    dashboardStatus.className = `status-text ${kind}`.trim();
  };

  async function loadScript(src, attrs = {}) {
    await new Promise((resolve, reject) => {
      const script = document.createElement("script");
      script.src = src;
      script.async = true;
      script.crossOrigin = "anonymous";
      Object.entries(attrs).forEach(([key, value]) => script.setAttribute(key, value));
      script.onload = resolve;
      script.onerror = () => reject(new Error("Clerk sign-in could not be loaded. Check your connection and try again."));
      document.head.appendChild(script);
    });
  }

  async function loadClerk() {
    const key = String(config.clerkPublishableKey || "");
    if (!key.startsWith("pk_") || !API) {
      authStatus.textContent = "Account sign-in is not configured for this deployment yet.";
      authStatus.classList.add("error-text");
      return;
    }
    try {
      const encoded = key.split("_")[2];
      const frontendApi = atob(encoded.replace(/-/g, "+").replace(/_/g, "/")).replace(/\$$/, "");
      if (!/^[a-z0-9.-]+$/i.test(frontendApi)) throw new Error("Invalid Clerk publishable key.");
      await loadScript(`https://${frontendApi}/npm/@clerk/ui@1/dist/ui.browser.js`);
      await loadScript(`https://${frontendApi}/npm/@clerk/clerk-js@6/dist/clerk.browser.js`, { "data-clerk-publishable-key": key });
      if (!window.Clerk) throw new Error("Clerk did not initialize. Check the account configuration.");
      clerk = window.Clerk;
      await clerk.load({
        ui: { ClerkUI: window.__internal_ClerkUICtor },
        appearance: {
          variables: { colorPrimary: "#ff962e", colorBackground: "#1d1d20", colorForeground: "#f5f2ed", colorInputBackground: "#101012", colorInputText: "#f5f2ed", borderRadius: "0.7rem" },
        },
      });
      await renderAuth();
      clerk.addListener(() => { void renderAuth(); });
    } catch (error) {
      authStatus.textContent = error instanceof Error ? error.message : "Unable to load account sign-in.";
      authStatus.classList.add("error-text");
    }
  }

  async function renderAuth() {
    if (!clerk) return;
    if (!clerk.isSignedIn) {
      dashboard.hidden = true;
      dashboard.dataset.loaded = "false";
      document.getElementById("user-button").hidden = true;
      document.getElementById("messages").replaceChildren();
      document.getElementById("favorites").replaceChildren();
      document.getElementById("tracks").replaceChildren();
      messages = [];
      favorites = [];
      tracks = [];
      authPanel.hidden = false;
      authStatus.textContent = "";
      const signIn = document.getElementById("sign-in");
      if (!signIn.dataset.mounted) {
        clerk.mountSignIn(signIn, { forceRedirectUrl: location.href, fallbackRedirectUrl: location.href });
        signIn.dataset.mounted = "true";
      }
      return;
    }
    authPanel.hidden = true;
    dashboard.hidden = false;
    const userButton = document.getElementById("user-button");
    userButton.hidden = false;
    if (!userButton.dataset.mounted) {
      clerk.mountUserButton(userButton);
      userButton.dataset.mounted = "true";
    }
    const user = clerk.user;
    const primaryEmail = user?.primaryEmailAddress?.emailAddress || user?.emailAddresses?.[0]?.emailAddress || "MeshCrossway account";
    document.getElementById("account-email").textContent = primaryEmail;
    if (!dashboard.dataset.loaded) {
      dashboard.dataset.loaded = "true";
      await loadDashboard();
    }
  }

  async function api(path, options = {}) {
    const token = await clerk.session?.getToken();
    if (!token) throw new Error("Your sign-in session expired. Sign out and sign in again.");
    const response = await fetch(`${API}${path}`, {
      ...options,
      headers: { Authorization: `Bearer ${token}`, ...(options.headers || {}) },
      cache: "no-store",
    });
    if (!response.ok) {
      const body = await response.json().catch(() => ({}));
      const detail = body.error ? String(body.error).replaceAll("_", " ").toLowerCase() : `HTTP ${response.status}`;
      throw new Error(detail);
    }
    return response;
  }

  async function loadAllMessages() {
    const all = [];
    let cursor = "";
    do {
      const query = cursor ? `?cursor=${encodeURIComponent(cursor)}` : "";
      const page = await (await api(`/v1/messages${query}`)).json();
      if (Array.isArray(page.messages)) all.push(...page.messages);
      cursor = page.nextCursor || "";
    } while (cursor && all.length < 20000);
    return all;
  }

  async function loadDashboard() {
    setStatus("Loading your account data…");
    try {
      const [messageRows, favoriteData, trackData] = await Promise.all([
        loadAllMessages(), api("/v1/favorites").then((r) => r.json()), api("/v1/tracks").then((r) => r.json()),
      ]);
      messages = messageRows;
      favorites = (favoriteData.favorites || []).filter((item) => Number(item.favorite) === 1 || item.favorite === true);
      tracks = trackData.tracks || [];
      renderMessages();
      renderFavorites();
      renderTracks(trackData.usage);
      setStatus("");
    } catch (error) {
      setStatus(`Could not load cloud data: ${error instanceof Error ? error.message : "request failed"}.`, "error-text");
    }
  }

  function renderMessages() {
    const target = document.getElementById("messages");
    document.getElementById("message-count").textContent = String(messages.length);
    if (!messages.length) {
      target.innerHTML = '<div class="empty-state">No synchronized messages were found in the current retention window. In the Android app, open Account &amp; Sync, tap Sync Now, then refresh this page.</div>';
      return;
    }
    target.innerHTML = messages.slice().reverse().map((row) => {
      const network = row.network || "MESHTASTIC";
      const conversation = row.conversation_id ?? row.conversationId ?? "";
      const sender = row.sender || (row.outgoing ? "You" : "Unknown sender");
      const date = formatDate(row.sent_at ?? row.sentAt);
      const title = row.title ? `${escapeHtml(row.title)} · ` : "";
      const peer = row.peer ? ` · ${escapeHtml(row.peer)}` : "";
      const state = row.state ? `<div class="message-meta">Message status · ${escapeHtml(row.state)}</div>` : "";
      return `<article class="message-card"><div class="message-meta"><span class="network-tag">${escapeHtml(networkName(network))}</span><time>${escapeHtml(date)}</time></div><div class="message-conversation">${title}Conversation · ${escapeHtml(conversation)}${peer}</div><p class="message-body"><span class="message-sender">${escapeHtml(sender)}:</span> ${escapeHtml(row.body)}</p>${state}</article>`;
    }).join("");
  }

  function renderFavorites() {
    const target = document.getElementById("favorites");
    document.getElementById("favorite-count").textContent = String(favorites.length);
    if (!favorites.length) {
      target.innerHTML = '<div class="empty-state">No favorite conversations are saved.</div>';
      return;
    }
    target.innerHTML = favorites.map((item) => {
      const network = item.network || "MESHTASTIC";
      const conversation = item.conversation_id ?? item.conversationId ?? "";
      return `<div class="favorite-row"><div class="favorite-name"><strong>${escapeHtml(conversation)}</strong><span>${escapeHtml(networkName(network))}</span></div><button class="small-button" type="button" data-unfavorite="${escapeHtml(network)}" data-conversation="${escapeHtml(conversation)}">Remove favorite</button></div>`;
    }).join("");
  }

  function renderTracks(usage = {}) {
    const target = document.getElementById("tracks");
    const count = Number(usage?.count ?? tracks.length);
    const bytes = Number(usage?.bytes || tracks.reduce((sum, item) => sum + Number(item.byte_size || 0), 0));
    const maxTracks = Number(usage?.maxTracks || 5);
    const maxBytes = Number(usage?.maxTotalBytes || 25 * 1024 * 1024);
    document.getElementById("quota-label").textContent = `${count} of ${maxTracks} tracks · ${formatBytes(bytes)} of ${formatBytes(maxBytes)}`;
    document.getElementById("quota-file-limit").textContent = `Maximum ${formatBytes(usage?.maxFileBytes || 5 * 1024 * 1024)} per track`;
    document.getElementById("quota-fill").style.width = `${Math.min(100, Math.max(0, bytes / maxBytes * 100))}%`;
    if (!tracks.length) {
      target.innerHTML = '<div class="empty-state">No GPX tracks are saved in the cloud account.</div>';
      return;
    }
    target.innerHTML = tracks.map((item) => `<article class="track-row">
      <div><div class="track-name">${escapeHtml(item.name || "Track")}</div><div class="track-meta"><span>${escapeHtml(formatBytes(item.byte_size))}</span><time>Updated ${escapeHtml(formatDate(item.updated_at))}</time></div></div>
      <div class="track-actions"><button class="small-button" type="button" data-track-action="download" data-track-id="${escapeHtml(item.track_id)}">Download</button><button class="small-button" type="button" data-track-action="rename" data-track-id="${escapeHtml(item.track_id)}" data-track-name="${escapeHtml(item.name || "Track")}">Rename</button><button class="small-button danger-button" type="button" data-track-action="delete" data-track-id="${escapeHtml(item.track_id)}">Delete</button></div>
    </article>`).join("");
  }

  async function refresh() {
    dashboard.dataset.loaded = "true";
    await loadDashboard();
  }

  async function unfavorite(network, conversationId, button) {
    button.disabled = true;
    try {
      await api("/v1/favorites", { method: "PUT", headers: { "content-type": "application/json" }, body: JSON.stringify({ network, conversationId, favorite: false }) });
      favorites = favorites.filter((item) => !((item.network || "MESHTASTIC") === network && (item.conversation_id ?? item.conversationId) === conversationId));
      renderFavorites();
      setStatus("Favorite removed. Its messages will follow the seven-day retention policy.", "success-text");
    } catch (error) {
      button.disabled = false;
      setStatus(`Could not update favorite: ${error.message}.`, "error-text");
    }
  }

  async function downloadTrack(trackId) {
    const item = tracks.find((track) => track.track_id === trackId);
    const response = await api(`/v1/tracks/${encodeURIComponent(trackId)}`);
    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = `${item?.name || "track"}.gpx`;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    URL.revokeObjectURL(url);
  }

  async function renameTrack(trackId, previousName) {
    const name = prompt("New track name", previousName);
    if (!name || name.trim() === previousName) return;
    if (name.trim().length > 120) { setStatus("Track names can contain up to 120 characters.", "error-text"); return; }
    try {
      const original = await api(`/v1/tracks/${encodeURIComponent(trackId)}`);
      const saved = await api(`/v1/tracks/${encodeURIComponent(trackId)}?name=${encodeURIComponent(name.trim())}`, {
        method: "PUT", headers: { "content-type": "application/gpx+xml; charset=utf-8" }, body: await original.blob(),
      });
      await saved.json();
      await refresh();
      setStatus("Track renamed.", "success-text");
    } catch (error) { setStatus(`Could not rename track: ${error.message}.`, "error-text"); }
  }

  async function deleteTrack(trackId) {
    const item = tracks.find((track) => track.track_id === trackId);
    if (!confirm(`Permanently delete the cloud copy of “${item?.name || "this track"}”? This does not delete a copy stored on your phone.`)) return;
    try {
      await api(`/v1/tracks/${encodeURIComponent(trackId)}`, { method: "DELETE" });
      tracks = tracks.filter((track) => track.track_id !== trackId);
      renderTracks({ count: tracks.length, bytes: tracks.reduce((sum, track) => sum + Number(track.byte_size || 0), 0), maxTracks: 5, maxTotalBytes: 25 * 1024 * 1024, maxFileBytes: 5 * 1024 * 1024 });
      setStatus("Cloud track deleted. The phone's local copy is unchanged.", "success-text");
    } catch (error) { setStatus(`Could not delete track: ${error.message}.`, "error-text"); }
  }

  async function uploadTrack(file) {
    if (!file) return;
    if (file.size > 5 * 1024 * 1024) { setStatus("This GPX is over the 5 MiB per-track limit.", "error-text"); return; }
    const name = file.name.replace(/\.gpx$/i, "").trim() || "Track";
    const trackId = crypto.randomUUID();
    setStatus("Uploading GPX…");
    try {
      await api(`/v1/tracks/${encodeURIComponent(trackId)}?name=${encodeURIComponent(name)}`, { method: "PUT", headers: { "content-type": "application/gpx+xml; charset=utf-8" }, body: file });
      await refresh();
      setStatus("GPX uploaded to your cloud account.", "success-text");
    } catch (error) { setStatus(`Could not upload GPX: ${error.message}.`, "error-text"); }
  }

  document.getElementById("refresh-button").addEventListener("click", () => { void refresh(); });
  document.getElementById("track-upload").addEventListener("change", (event) => {
    const input = event.currentTarget;
    void uploadTrack(input.files?.[0]);
    input.value = "";
  });
  document.getElementById("favorites").addEventListener("click", (event) => {
    const button = event.target.closest("[data-unfavorite]");
    if (button) void unfavorite(button.dataset.unfavorite, button.dataset.conversation, button);
  });
  document.getElementById("tracks").addEventListener("click", (event) => {
    const button = event.target.closest("[data-track-action]");
    if (!button) return;
    const { trackAction, trackId } = button.dataset;
    button.disabled = true;
    const action = trackAction === "download" ? downloadTrack(trackId) : trackAction === "rename" ? renameTrack(trackId, button.dataset.trackName) : deleteTrack(trackId);
    Promise.resolve(action).catch((error) => setStatus(error.message || "Track operation failed.", "error-text")).finally(() => { if (button.isConnected) button.disabled = false; });
  });

  void loadClerk();
})();
