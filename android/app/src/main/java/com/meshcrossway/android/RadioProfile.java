package com.meshcrossway.android;

import java.util.UUID;

public enum RadioProfile {
    MESHTASTIC("Meshtastic", "6ba1b218-15a8-461f-9fa8-5dcae273eafd", "f75c76d2-129e-4dad-a1dd-7866124401e7", "2c55e69e-4993-11ed-b878-0242ac120002", "ed9da18c-a800-4f66-a670-aa7547e34453"),
    MESHCORE("MeshCore", "6e400001-b5a3-f393-e0a9-e50e24dcca9e", "6e400002-b5a3-f393-e0a9-e50e24dcca9e", "6e400003-b5a3-f393-e0a9-e50e24dcca9e", "6e400003-b5a3-f393-e0a9-e50e24dcca9e");
    public final String title;
    public final UUID service, write, read, notify;
    public static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    RadioProfile(String title, String service, String write, String read, String notify) {
        this.title=title; this.service=UUID.fromString(service); this.write=UUID.fromString(write);
        this.read=UUID.fromString(read); this.notify=UUID.fromString(notify);
    }
}
