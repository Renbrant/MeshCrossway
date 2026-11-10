interface Env {
  DB: D1Database;
  TRACKS: R2Bucket;
  CLERK_ISSUER: string;
  CLERK_JWKS_URL?: string;
  WEB_ORIGIN?: string;
}

type Network = "MESHTASTIC" | "MESHCORE";
interface SessionClaims { sub: string; iss: string; exp: number; nbf?: number; }
interface CloudMessage {
  network: Network;
  conversationId: string;
  messageId: string;
  sentAt: number;
  sender: string;
  body: string;
  outgoing: boolean;
  read: boolean;
}

const SEVEN_DAYS = 7 * 24 * 60 * 60;
const MAX_TRACKS = 5;
const MAX_TRACK_BYTES = 5 * 1024 * 1024;
const MAX_TOTAL_TRACK_BYTES = MAX_TRACKS * MAX_TRACK_BYTES;
const MAX_MESSAGE_BATCH = 100;
const MAX_MESSAGE_BODY = 4096;
const MAX_TRACK_NAME = 120;
const jwksCache = new Map<string, { expires: number; keys: JsonWebKey[] }>();

const json = (data: unknown, status = 200): Response => new Response(JSON.stringify(data), {
  status,
  headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
});
const error = (code: string, status: number): Response => json({ error: code }, status);
const validNetwork = (value: unknown): value is Network => value === "MESHTASTIC" || value === "MESHCORE";
const validId = (value: unknown): value is string => typeof value === "string" && /^[a-zA-Z0-9._:-]{1,160}$/.test(value);
type MessageCursor = { time: number; network: Network; conversationId: string; messageId: string };

function decodeBase64Url(value: string): Uint8Array {
  const padded = value.replace(/-/g, "+").replace(/_/g, "/").padEnd(Math.ceil(value.length / 4) * 4, "=");
  const binary = atob(padded);
  return Uint8Array.from(binary, char => char.charCodeAt(0));
}

function decodeCursor(value: string | null): MessageCursor {
  if (!value) return { time: 0, network: "MESHTASTIC", conversationId: "", messageId: "" };
  try {
    const cursor = JSON.parse(new TextDecoder().decode(decodeBase64Url(value))) as MessageCursor;
    if (!Number.isSafeInteger(cursor.time) || cursor.time < 0 || !validNetwork(cursor.network) || !validId(cursor.conversationId) || !validId(cursor.messageId)) throw new Error();
    return cursor;
  } catch { throw new Error("INVALID_CURSOR"); }
}

function encodeCursor(cursor: MessageCursor): string {
  const bytes = new TextEncoder().encode(JSON.stringify(cursor));
  let binary = "";
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

async function verifyToken(request: Request, env: Env): Promise<string | null> {
  const auth = request.headers.get("authorization") || "";
  const match = /^Bearer ([A-Za-z0-9._-]+)$/.exec(auth);
  if (!match || !env.CLERK_ISSUER || env.CLERK_ISSUER.startsWith("SET_")) return null;
  const parts = match[1].split(".");
  if (parts.length !== 3) return null;
  try {
    const header = JSON.parse(new TextDecoder().decode(decodeBase64Url(parts[0]))) as { alg?: string; kid?: string };
    if (header.alg !== "RS256" || !header.kid) return null;
    const jwksUrl = env.CLERK_JWKS_URL || `${env.CLERK_ISSUER.replace(/\/$/, "")}/.well-known/jwks.json`;
    let cached = jwksCache.get(jwksUrl);
    if (!cached || cached.expires < Date.now()) {
      const response = await fetch(jwksUrl, { headers: { accept: "application/json" } });
      if (!response.ok) return null;
      const value = await response.json() as { keys?: JsonWebKey[] };
      if (!Array.isArray(value.keys)) return null;
      cached = { keys: value.keys, expires: Date.now() + 60 * 60 * 1000 };
      jwksCache.set(jwksUrl, cached);
    }
    const jwk = cached.keys.find(key => (key as JsonWebKey & { kid?: string }).kid === header.kid);
    if (!jwk) return null;
    const key = await crypto.subtle.importKey("jwk", jwk, { name: "RSASSA-PKCS1-v1_5", hash: "SHA-256" }, false, ["verify"]);
    const signingInput = new TextEncoder().encode(`${parts[0]}.${parts[1]}`);
    const signatureOk = await crypto.subtle.verify("RSASSA-PKCS1-v1_5", key, decodeBase64Url(parts[2]), signingInput);
    if (!signatureOk) return null;
    const claims = JSON.parse(new TextDecoder().decode(decodeBase64Url(parts[1]))) as SessionClaims;
    const now = Math.floor(Date.now() / 1000);
    if (claims.iss !== env.CLERK_ISSUER || typeof claims.sub !== "string" || !claims.sub || claims.exp <= now || (claims.nbf && claims.nbf > now)) return null;
    return claims.sub;
  } catch { return null; }
}

function cors(request: Request, env: Env): Headers | null {
  const origin = request.headers.get("origin");
  if (!origin) return new Headers(); // Native app requests do not send Origin.
  if (!env.WEB_ORIGIN || env.WEB_ORIGIN.startsWith("https://SET_") || origin !== env.WEB_ORIGIN) return null;
  return new Headers({ "access-control-allow-origin": origin, "access-control-allow-methods": "GET,POST,PUT,DELETE,OPTIONS", "access-control-allow-headers": "authorization,content-type", "vary": "Origin" });
}

async function readJson(request: Request, limit: number): Promise<any | null> {
  const length = Number(request.headers.get("content-length") || 0);
  if (length > limit) return null;
  if (!request.body) return null;
  const reader = request.body.getReader();
  const chunks: Uint8Array[] = [];
  let size = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > limit) { await reader.cancel(); return null; }
      chunks.push(value);
    }
  } catch { return null; }
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
  try { return JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(bytes)); } catch { return null; }
}

async function handleMessages(request: Request, env: Env, userId: string): Promise<Response> {
  const cutoff = Math.floor(Date.now() / 1000) - SEVEN_DAYS;
  if (request.method === "GET") {
    const url = new URL(request.url);
    let cursor: MessageCursor;
    try { cursor = decodeCursor(url.searchParams.get("cursor")); } catch { return error("INVALID_CURSOR", 400); }
    cursor.time = Math.min(cursor.time, Math.floor(Date.now() / 1000));
    const { results } = await env.DB.prepare(`SELECT m.network,m.conversation_id,m.message_id,m.sent_at,m.sender,m.body,m.outgoing,m.is_read,c.favorite
      FROM messages m LEFT JOIN conversations c ON c.user_id=m.user_id AND c.network=m.network AND c.conversation_id=m.conversation_id
      WHERE m.user_id=? AND ((m.sent_at>=? AND (m.sent_at,m.network,m.conversation_id,m.message_id)>(?,?,?,?)) OR
        (c.favorite=1 AND (m.sent_at,m.network,m.conversation_id,m.message_id)>(?,?,?,?)))
      ORDER BY m.sent_at ASC,m.network ASC,m.conversation_id ASC,m.message_id ASC LIMIT 1001`)
      .bind(userId, cutoff, cursor.time, cursor.network, cursor.conversationId, cursor.messageId, cursor.time, cursor.network, cursor.conversationId, cursor.messageId).all();
    const hasMore = results.length > 1000;
    const page = hasMore ? results.slice(0, 1000) : results;
    const last = page[page.length - 1] as {sent_at:number;network:Network;conversation_id:string;message_id:string} | undefined;
    const nextCursor = hasMore && last ? encodeCursor({ time: last.sent_at, network: last.network, conversationId: last.conversation_id, messageId: last.message_id }) : null;
    return json({ cutoff, messages: page, nextCursor });
  }
  if (request.method !== "POST") return error("METHOD_NOT_ALLOWED", 405);
  const payload = await readJson(request, 1024 * 1024);
  if (!payload || !Array.isArray(payload.messages) || payload.messages.length > MAX_MESSAGE_BATCH) return error("INVALID_BATCH", 400);
  const now = Math.floor(Date.now() / 1000);
  const statements: D1PreparedStatement[] = [];
  for (const message of payload.messages as CloudMessage[]) {
    if (!validNetwork(message.network) || !validId(message.conversationId) || !validId(message.messageId) ||
      !Number.isSafeInteger(message.sentAt) || message.sentAt > now + 300 || typeof message.sender !== "string" || message.sender.length > 160 ||
      typeof message.body !== "string" || message.body.length > MAX_MESSAGE_BODY || typeof message.outgoing !== "boolean" || typeof message.read !== "boolean") return error("INVALID_MESSAGE", 400);
    const bodyBytes = new TextEncoder().encode(message.body).byteLength;
    if (bodyBytes > MAX_MESSAGE_BODY * 4) return error("MESSAGE_TOO_LARGE", 413);
    statements.push(env.DB.prepare(`INSERT INTO messages(user_id,network,conversation_id,message_id,sent_at,sender,body,outgoing,is_read)
      SELECT ?,?,?,?,?,?,?,?,? WHERE ? >= ? OR EXISTS(SELECT 1 FROM conversations WHERE user_id=? AND network=? AND conversation_id=? AND favorite=1)
      ON CONFLICT(user_id,network,conversation_id,message_id) DO UPDATE SET sender=excluded.sender,body=excluded.body,outgoing=excluded.outgoing,is_read=excluded.is_read`)
      .bind(userId, message.network, message.conversationId, message.messageId, message.sentAt, message.sender, message.body, message.outgoing ? 1 : 0, message.read ? 1 : 0,
        cutoff, message.sentAt, userId, message.network, message.conversationId));
  }
  // Old non-favorite messages are pruned; favorites retain their complete conversation.
  statements.push(env.DB.prepare(`DELETE FROM messages WHERE user_id=? AND sent_at<? AND NOT EXISTS(
    SELECT 1 FROM conversations c WHERE c.user_id=messages.user_id AND c.network=messages.network AND c.conversation_id=messages.conversation_id AND c.favorite=1)`)
    .bind(userId, cutoff));
  await env.DB.batch(statements);
  return json({ accepted: payload.messages.length, retentionDays: 7 });
}

async function handleFavorites(request: Request, env: Env, userId: string): Promise<Response> {
  if (request.method === "GET") {
    const { results } = await env.DB.prepare("SELECT network,conversation_id,updated_at FROM conversations WHERE user_id=? AND favorite=1 ORDER BY updated_at DESC").bind(userId).all();
    return json({ favorites: results });
  }
  if (request.method !== "PUT") return error("METHOD_NOT_ALLOWED", 405);
  const value = await readJson(request, 8192);
  if (!value || !validNetwork(value.network) || !validId(value.conversationId) || typeof value.favorite !== "boolean") return error("INVALID_FAVORITE", 400);
  const now = Math.floor(Date.now() / 1000);
  if (value.favorite) {
    await env.DB.prepare(`INSERT INTO conversations(user_id,network,conversation_id,favorite,updated_at) VALUES(?,?,?,1,?)
      ON CONFLICT(user_id,network,conversation_id) DO UPDATE SET favorite=1,updated_at=excluded.updated_at`)
      .bind(userId, value.network, value.conversationId, now).run();
  } else {
    await env.DB.batch([
      env.DB.prepare("UPDATE conversations SET favorite=0,updated_at=? WHERE user_id=? AND network=? AND conversation_id=?").bind(now, userId, value.network, value.conversationId),
      env.DB.prepare("DELETE FROM messages WHERE user_id=? AND network=? AND conversation_id=? AND sent_at<?").bind(userId, value.network, value.conversationId, now - SEVEN_DAYS)
    ]);
  }
  return json({ saved: true, permanent: value.favorite });
}

async function handleTracks(request: Request, env: Env, userId: string, trackId: string | null): Promise<Response> {
  if (request.method === "GET" && !trackId) {
    const { results } = await env.DB.prepare("SELECT track_id,name,byte_size,updated_at FROM tracks WHERE user_id=? ORDER BY updated_at DESC").bind(userId).all();
    const usage = await env.DB.prepare("SELECT COUNT(*) AS count,COALESCE(SUM(byte_size),0) AS bytes FROM tracks WHERE user_id=?").bind(userId).first<{count:number;bytes:number}>();
    return json({ tracks: results, usage: { count: usage?.count || 0, bytes: usage?.bytes || 0, maxTracks: MAX_TRACKS, maxFileBytes: MAX_TRACK_BYTES, maxTotalBytes: MAX_TOTAL_TRACK_BYTES } });
  }
  if (!trackId || !validId(trackId)) return error("INVALID_TRACK_ID", 400);
  const key = `${userId}/${trackId}/${crypto.randomUUID()}.gpx`;
  if (request.method === "GET") {
    const row = await env.DB.prepare("SELECT object_key,name,byte_size FROM tracks WHERE user_id=? AND track_id=?").bind(userId, trackId).first<{object_key:string;name:string;byte_size:number}>();
    if (!row) return error("TRACK_NOT_FOUND", 404);
    const object = await env.TRACKS.get(row.object_key);
    if (!object) return error("TRACK_NOT_FOUND", 404);
    const headers = new Headers({ "content-type": "application/gpx+xml; charset=utf-8", "content-length": String(row.byte_size), "cache-control": "no-store", "content-disposition": `attachment; filename*=UTF-8''${encodeURIComponent(row.name)}.gpx` });
    return new Response(object.body, { headers });
  }
  if (request.method === "DELETE") {
    const row = await env.DB.prepare("SELECT object_key FROM tracks WHERE user_id=? AND track_id=?").bind(userId, trackId).first<{object_key:string}>();
    if (!row) return error("TRACK_NOT_FOUND", 404);
    await env.DB.prepare("DELETE FROM tracks WHERE user_id=? AND track_id=?").bind(userId, trackId).run();
    await env.TRACKS.delete(row.object_key);
    return json({ deleted: true });
  }
  if (request.method !== "PUT") return error("METHOD_NOT_ALLOWED", 405);
  if (!/^application\/gpx\+xml(?:;\s*charset=utf-8)?$/i.test(request.headers.get("content-type") || "")) return error("UNSUPPORTED_MEDIA_TYPE", 415);
  if (Number(request.headers.get("content-length") || 0) > MAX_TRACK_BYTES) return json({ error: "TRACK_FILE_LIMIT", maxBytes: MAX_TRACK_BYTES }, 413);
  if (!request.body) return error("EMPTY_TRACK", 400);
  const reader = request.body.getReader();
  const chunks: Uint8Array[] = [];
  let byteSize = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      byteSize += value.byteLength;
      if (byteSize > MAX_TRACK_BYTES) { await reader.cancel(); return json({ error: "TRACK_FILE_LIMIT", maxBytes: MAX_TRACK_BYTES }, 413); }
      chunks.push(value);
    }
  } catch { return error("TRACK_UPLOAD_INCOMPLETE", 400); }
  if (!byteSize) return error("EMPTY_TRACK", 400);
  const bytes = new Uint8Array(byteSize);
  let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
  const name = new URL(request.url).searchParams.get("name") || "Track";
  if (name.length > MAX_TRACK_NAME) return error("INVALID_TRACK_NAME", 400);
  let text: string;
  try { text = new TextDecoder("utf-8", { fatal: true }).decode(bytes); } catch { return error("INVALID_GPX", 400); }
  if (!text.startsWith("<?xml") || !text.includes("<gpx") || !text.includes("</gpx>")) return error("INVALID_GPX", 400);
  const old = await env.DB.prepare("SELECT object_key FROM tracks WHERE user_id=? AND track_id=?").bind(userId, trackId).first<{object_key:string}>();
    await env.TRACKS.put(key, bytes, { httpMetadata: { contentType: "application/gpx+xml" }, customMetadata: { userId, trackId } });
  try {
    const result = await env.DB.prepare(`INSERT INTO tracks(user_id,track_id,name,object_key,byte_size,updated_at) VALUES(?,?,?,?,?,?)
      ON CONFLICT(user_id,track_id) DO UPDATE SET name=excluded.name,object_key=excluded.object_key,byte_size=excluded.byte_size,updated_at=excluded.updated_at`)
      .bind(userId, trackId, name, key, bytes.byteLength, Math.floor(Date.now() / 1000)).run();
    if (result.success === false) throw new Error("TRACK_WRITE_FAILED");
  } catch (e) {
    await env.TRACKS.delete(key);
    const message = e instanceof Error ? e.message : "";
    if (message.includes("TRACK_COUNT_LIMIT")) return json({ error: "TRACK_COUNT_LIMIT", maxTracks: MAX_TRACKS }, 409);
    if (message.includes("TRACK_FILE_LIMIT")) return json({ error: "TRACK_FILE_LIMIT", maxBytes: MAX_TRACK_BYTES }, 413);
    if (message.includes("TRACK_STORAGE_LIMIT")) return json({ error: "TRACK_STORAGE_LIMIT", maxBytes: MAX_TOTAL_TRACK_BYTES }, 409);
    return error("TRACK_SAVE_FAILED", 500);
  }
  if (old && old.object_key !== key) await env.TRACKS.delete(old.object_key);
  return json({ saved: true, byteSize: bytes.byteLength, maxFileBytes: MAX_TRACK_BYTES, maxTracks: MAX_TRACKS });
}

const worker = {
  async fetch(request: Request, env: Env): Promise<Response> {
    const headers = cors(request, env);
    if (!headers) return error("ORIGIN_NOT_ALLOWED", 403);
    if (request.method === "OPTIONS") return new Response(null, { status: 204, headers });
    const url = new URL(request.url);
    if (url.pathname === "/health" && request.method === "GET") return json({ ok: true, service: "meshcrossway-cloud" });
    const userId = await verifyToken(request, env);
    if (!userId) return error("UNAUTHORIZED", 401);
    let response: Response;
    if (url.pathname === "/v1/messages") response = await handleMessages(request, env, userId);
    else if (url.pathname === "/v1/favorites") response = await handleFavorites(request, env, userId);
    else if (url.pathname === "/v1/tracks") response = await handleTracks(request, env, userId, null);
    else {
      const match = /^\/v1\/tracks\/([^/]+)$/.exec(url.pathname);
      response = match ? await handleTracks(request, env, userId, decodeURIComponent(match[1])) : error("NOT_FOUND", 404);
    }
    headers.forEach((value, key) => response.headers.set(key, value));
    return response;
  },
  async scheduled(_event: ScheduledController, env: Env, ctx: ExecutionContext): Promise<void> {
    const cutoff = Math.floor(Date.now() / 1000) - SEVEN_DAYS;
    ctx.waitUntil(env.DB.prepare(`DELETE FROM messages WHERE sent_at<? AND NOT EXISTS(
      SELECT 1 FROM conversations c WHERE c.user_id=messages.user_id AND c.network=messages.network AND c.conversation_id=messages.conversation_id AND c.favorite=1)`)
      .bind(cutoff).run());
  }
};

export default worker;
