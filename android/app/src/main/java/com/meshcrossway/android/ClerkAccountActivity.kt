package com.meshcrossway.android

import android.app.Activity
import android.content.ComponentName
import android.content.Intent
import android.content.ServiceConnection
import android.graphics.Color
import android.os.Bundle
import android.os.IBinder
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast

class ClerkAccountActivity : Activity() {
    private val pageBackground = Color.rgb(9, 12, 16)
    private val panel = Color.rgb(24, 29, 36)
    private val orange = Color.rgb(255, 138, 36)
    private val white = Color.rgb(242, 244, 247)
    private val muted = Color.rgb(154, 166, 180)
    private var radioService: RadioService? = null
    private var bound = false
    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            radioService = (binder as RadioService.LocalBinder).service()
            render()
        }
        override fun onServiceDisconnected(name: ComponentName) { radioService = null }
    }

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        window.statusBarColor = pageBackground
        window.navigationBarColor = pageBackground
        bound = bindService(Intent(this, RadioService::class.java), serviceConnection, BIND_AUTO_CREATE)
        render()
    }

    override fun onDestroy() {
        if (bound) unbindService(serviceConnection)
        super.onDestroy()
    }

    private fun render() {
        val density = resources.displayMetrics.density
        fun dp(value: Int) = (value * density + 0.5f).toInt()
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(18), dp(20), dp(24))
            setBackgroundColor(pageBackground)
        }
        val header = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        val title = text("Account & sync", 22, white).apply { setTypeface(null, android.graphics.Typeface.BOLD) }
        header.addView(title, LinearLayout.LayoutParams(0, dp(52), 1f))
        val back = Button(this).apply { text = "Back"; setOnClickListener { finish() } }
        header.addView(back)
        root.addView(header)
        root.addView(text("MESHCROSSWAY ACCOUNT", 12, orange), LinearLayout.LayoutParams(-1, dp(34)))

        if (!ClerkBridge.isConfigured()) {
            root.addView(cardText("This build does not have a Clerk publishable key. Sign-in becomes available after the Clerk application is configured for the app."))
            setContentView(root)
            return
        }

        if (ClerkBridge.isSignedIn()) {
            root.addView(cardText("Your Clerk account is connected on this device."))
            root.addView(actionButton("Sign out") {
                ClerkBridge.signOut(Runnable {
                    Toast.makeText(this, "Signed out", Toast.LENGTH_SHORT).show()
                    render()
                })
            })
        } else {
            root.addView(cardText("Sign in or create an account using the providers enabled for MeshCrossway in Clerk."))
            root.addView(actionButton("Sign in or create account") {
                ClerkBridge.signIn(object : ClerkBridge.ResultCallback {
                    override fun onComplete(success: Boolean) {
                        if (success) Toast.makeText(this@ClerkAccountActivity, "Account connected", Toast.LENGTH_SHORT).show()
                        else Toast.makeText(this@ClerkAccountActivity, "Sign-in closed or could not be completed", Toast.LENGTH_LONG).show()
                        render()
                    }
                })
            })
        }

        root.addView(text("CLOUD STORAGE", 12, orange).apply { setPadding(0, dp(22), 0, dp(6)) })
        val syncPrefs = getSharedPreferences("cloud_sync", MODE_PRIVATE)
        val enabled = syncPrefs.getBoolean("enabled", false)
        root.addView(Switch(this).apply {
            text = "Enable cloud synchronization"
            textSize = 15f
            setTextColor(white)
            isChecked = enabled
            isEnabled = ClerkBridge.isSignedIn()
            setOnCheckedChangeListener { _, checked ->
                syncPrefs.edit().putBoolean("enabled", checked).apply()
                render()
            }
        })
        root.addView(cardText("This first sync stage includes conversations from the last seven days and favorite chats retained permanently. It does not upload Bluetooth identifiers, location, contacts, or radio settings. Ordinary messages older than seven days are removed from cloud storage."))
        root.addView(text("Cloud content is sent over HTTPS and stored in your Cloudflare account under your Clerk identity. It is not end-to-end encrypted. Turning sync off stops future syncs but does not delete data already stored in the cloud.", 13, muted).apply {
            setPadding(dp(4), dp(10), dp(4), 0)
        })
        root.addView(text("Free track storage is planned as up to five GPX tracks, 5 MiB each. Track sync will be added in a later step.", 13, muted).apply {
            setPadding(dp(4), dp(12), dp(4), 0)
        })
        if (enabled && !ClerkBridge.isSignedIn()) root.addView(text("Sign in to enable synchronization on this device.", 13, muted).apply { setPadding(dp(4), dp(10), dp(4), 0) })
        if (!CloudSyncClient.configured()) {
            root.addView(cardText("The cloud API is not configured in this build yet. No data will be uploaded until the Worker is deployed and its URL is added to the Android build."))
        } else if (enabled && ClerkBridge.isSignedIn()) {
            root.addView(actionButton("Sync now") {
                CloudSyncClient.sync(this, radioService) { message, success ->
                    runOnUiThread {
                        Toast.makeText(this, message, if (success) Toast.LENGTH_LONG else Toast.LENGTH_SHORT).show()
                        render()
                    }
                }
            }.apply { isEnabled = radioService != null })
            if (radioService == null) root.addView(text("Preparing local history…", 13, muted).apply { setPadding(dp(4), dp(8), dp(4), 0) })
        }
        setContentView(root)
    }

    private fun cardText(value: String): TextView {
        val density = resources.displayMetrics.density
        fun dp(amount: Int) = (amount * density + 0.5f).toInt()
        return text(value, 14, muted).apply {
            setPadding(dp(16), dp(16), dp(16), dp(16))
            setBackgroundColor(panel)
        }
    }

    private fun actionButton(label: String, action: () -> Unit): Button = Button(this).apply {
        text = label
        setTextColor(white)
        setBackgroundColor(orange)
        setOnClickListener { action() }
        layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            val density = resources.displayMetrics.density
            topMargin = (12 * density + 0.5f).toInt()
        }
    }

    private fun text(value: String, size: Int, color: Int) = TextView(this).apply {
        text = value
        textSize = size.toFloat()
        setTextColor(color)
    }
}
