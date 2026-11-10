package com.meshcrossway.android

import com.clerk.api.Clerk
import com.clerk.api.auth.HostedAuthMode
import com.clerk.api.network.serialization.onFailure
import com.clerk.api.network.serialization.onSuccess
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

/** Small Java-friendly boundary around Clerk's coroutine-based Android SDK. */
object ClerkBridge {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    @JvmStatic
    fun isConfigured(): Boolean = BuildConfig.CLERK_PUBLISHABLE_KEY.isNotBlank()

    @JvmStatic
    fun isSignedIn(): Boolean = isConfigured() && Clerk.session != null

    interface ResultCallback { fun onComplete(success: Boolean) }
    interface TokenCallback { fun onToken(token: String?) }

    @JvmStatic
    fun signIn(callback: ResultCallback) {
        scope.launch {
            val result = Clerk.auth.startHostedAuth(mode = HostedAuthMode.SIGN_IN)
            var success = false
            result.onSuccess { success = true }
            callback.onComplete(success)
        }
    }

    @JvmStatic
    fun signOut(callback: Runnable) {
        scope.launch {
            Clerk.auth.signOut()
            callback.run()
        }
    }

    @JvmStatic
    fun sessionToken(callback: TokenCallback) {
        scope.launch {
            Clerk.auth.getToken()
                .onSuccess { callback.onToken(it) }
                .onFailure { callback.onToken(null) }
        }
    }
}
