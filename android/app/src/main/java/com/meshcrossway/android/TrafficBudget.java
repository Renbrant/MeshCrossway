package com.meshcrossway.android;

import java.util.concurrent.atomic.AtomicLong;

/** Streaming reservation: concurrent map requests cannot consume more than this download budget. */
final class TrafficBudget {
    private final AtomicLong remaining;
    TrafficBudget(long bytes){remaining=new AtomicLong(Math.max(0,bytes));}
    long reserve(long requested){while(true){long old=remaining.get(),grant=Math.min(old,Math.max(0,requested));if(remaining.compareAndSet(old,old-grant))return grant;}}
    void refund(long bytes){if(bytes>0)remaining.addAndGet(bytes);}
}
