package com.meshcrossway.android;

import java.util.*;

/** Serializes initial BLE handshakes only; established data sessions remain independent. */
final class ConnectionGate {
    private RadioProfile owner;private final ArrayDeque<RadioProfile> waiting=new ArrayDeque<>();
    boolean acquire(RadioProfile p){if(owner==null||owner==p){owner=p;waiting.remove(p);return true;}if(!waiting.contains(p))waiting.add(p);return false;}
    RadioProfile release(RadioProfile p){waiting.remove(p);if(owner!=p)return null;owner=waiting.poll();return owner;}
    RadioProfile owner(){return owner;}
    void clear(){owner=null;waiting.clear();}
}
