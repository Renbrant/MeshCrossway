package com.meshcrossway.android

import android.app.Application
import com.clerk.api.Clerk

class MeshCrosswayApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        val key = BuildConfig.CLERK_PUBLISHABLE_KEY
        if (key.isNotBlank()) {
            Clerk.initialize(this, publishableKey = key)
        }
    }
}
