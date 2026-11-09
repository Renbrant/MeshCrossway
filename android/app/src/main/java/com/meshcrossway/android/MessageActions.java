package com.meshcrossway.android;

import java.util.ArrayList;
import java.util.Locale;
import static com.meshcrossway.android.MeshData.*;

/**
 * Presentation-only message helpers. Never rewrite received payloads or invent
 * radio metadata: drafts made here are ordinary text, not protocol ACKs.
 */
final class MessageActions {
    private MessageActions() {}

    static String displayBody(RadioProfile network, Message m) {
        if (m == null || m.body == null) return "";
        if (network != RadioProfile.MESHCORE || m.outgoing || !m.group) return m.body;
        String name = m.sender == null ? "" : m.sender.trim();
        if (name.isEmpty() || name.equalsIgnoreCase("Channel participant")) return m.body;
        String prefix = name + ": ";
        return m.body.startsWith(prefix) ? m.body.substring(prefix.length()) : m.body;
    }

    static String mention(String name) {
        if (name == null) return "";
        String clean = name.trim().replace('\n', ' ').replace('\r', ' ')
                .replace('[', ' ').replace(']', ' ').trim();
        if (clean.isEmpty() || clean.equalsIgnoreCase("Channel participant")
                || clean.equalsIgnoreCase("You")) return "";
        return "@[" + clean + "]";
    }

    static boolean hasReception(RxInfo rx) {
        return rx != null && (rx.hops >= 0 || Double.isFinite(rx.snr)
                || rx.rssi != Integer.MIN_VALUE || (rx.observed && !rx.path.isEmpty()));
    }

    /**
     * WADAMESH-like signal report prepared as an editable draft. Bluetooth RSSI
     * is not substituted for the original packet's LoRa reception metadata.
     */
    static String receptionReply(Message m) {
        StringBuilder reply = new StringBuilder("ACK");
        if (m.group) {
            String address = mention(m.sender);
            if (!address.isEmpty()) reply.append(' ').append(address);
        }
        reply.append(": ");
        RxInfo rx = m.rx;
        ArrayList<String> facts = new ArrayList<>();
        if (rx != null) {
            if (Double.isFinite(rx.snr))
                facts.add(String.format(Locale.ROOT, "SNR %.2f dB", rx.snr));
            if (rx.rssi != Integer.MIN_VALUE) facts.add("RSSI " + rx.rssi + " dBm");
            if (rx.hops >= 0) facts.add(rx.hops + " hop" + (rx.hops == 1 ? "" : "s"));
            if (rx.observed && !rx.path.isEmpty() && rx.width >= 1 && rx.width <= 3) {
                int width = rx.width * 2;
                int stops = rx.path.length() / width;
                int take = Math.min(stops, 6);
                ArrayList<String> hops = new ArrayList<>();
                for (int i = 0; i < take; i++)
                    hops.add(rx.path.substring(i * width, (i + 1) * width).toUpperCase(Locale.ROOT));
                if (!hops.isEmpty()) facts.add("observed via " + String.join(", ", hops)
                        + (stops > take ? " (+" + (stops - take) + " more)" : ""));
            }
        }
        reply.append(facts.isEmpty() ? "received (reception data unavailable)" : String.join(", ", facts));
        return reply.toString();
    }
}
