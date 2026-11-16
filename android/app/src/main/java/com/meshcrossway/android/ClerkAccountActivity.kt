package com.meshcrossway.android

import android.app.Activity
import android.app.AlertDialog
import android.content.ComponentName
import android.content.Intent
import android.content.ServiceConnection
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.view.Gravity
import android.view.ViewGroup
import android.view.WindowInsets
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
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
    private var trackService: TrackService? = null
    private var trackBound = false
    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            radioService = (binder as RadioService.LocalBinder).service()
            render()
        }
        override fun onServiceDisconnected(name: ComponentName) { radioService = null }
    }
    private val trackServiceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            trackService = (binder as TrackService.LocalBinder).service()
            render()
        }
        override fun onServiceDisconnected(name: ComponentName) { trackService = null }
    }

    @Suppress("DEPRECATION")
    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        if (Build.VERSION.SDK_INT < 35) {
            window.statusBarColor = pageBackground
            window.navigationBarColor = pageBackground
        }
        bound = bindService(Intent(this, RadioService::class.java), serviceConnection, BIND_AUTO_CREATE)
        trackBound = bindService(Intent(this, TrackService::class.java), trackServiceConnection, BIND_AUTO_CREATE)
        render()
    }

    override fun onDestroy() {
        if (bound) unbindService(serviceConnection)
        if (trackBound) unbindService(trackServiceConnection)
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
        root.addView(text("Privacy policy", 14, orange).apply {
            setPadding(0, 0, 0, dp(10))
            setOnClickListener { startActivity(Intent(Intent.ACTION_VIEW, android.net.Uri.parse("https://meshcrossway.renbrant.com/privacy/"))) }
        })

        if (!ClerkBridge.isConfigured()) {
            root.addView(cardText("This build does not have a Clerk publishable key. Sign-in becomes available after the Clerk application is configured for the app."))
            show(root)
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
            root.addView(actionButton("Delete account") { confirmDeleteAccount() }.apply { setBackgroundColor(Color.rgb(170, 40, 40)) })
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
        val tracksEnabled = syncPrefs.getBoolean("tracks_enabled", false)
        if (enabled) root.addView(Switch(this).apply {
            text = "Sync GPS tracks"
            textSize = 15f
            setTextColor(white)
            isChecked = tracksEnabled
            isEnabled = ClerkBridge.isSignedIn()
            setOnCheckedChangeListener { _, checked ->
                syncPrefs.edit().putBoolean("tracks_enabled", checked).apply()
                render()
            }
        })
        if (enabled) root.addView(cardText(if (tracksEnabled)
            "GPS track sync is on. Sync now uploads saved routes as GPX and downloads them to this device. Up to five tracks are stored in the cloud, maximum 5 MiB each. Active or paused recordings are not uploaded. Routes reveal location history; cloud copies are not end-to-end encrypted. Turning this off stops future track syncs but does not delete cloud copies. Deleting a track only on this phone also leaves its cloud copy, which may download again later."
        else
            "GPS tracks contain route and location data. Enable this separately to sync saved GPX tracks. The cloud limit is five tracks, up to 5 MiB each. Cloud copies are not end-to-end encrypted."))
        if (enabled && !ClerkBridge.isSignedIn()) root.addView(text("Sign in to enable synchronization on this device.", 13, muted).apply { setPadding(dp(4), dp(10), dp(4), 0) })
        if (!CloudSyncClient.configured()) {
            root.addView(cardText("The cloud API is not configured in this build yet. No data will be uploaded until the Worker is deployed and its URL is added to the Android build."))
        } else if (enabled && ClerkBridge.isSignedIn()) {
            root.addView(actionButton("Sync now") {
                CloudSyncClient.sync(this, radioService, trackService) { message, success ->
                    runOnUiThread {
                        Toast.makeText(this, message, if (success) Toast.LENGTH_LONG else Toast.LENGTH_SHORT).show()
                        render()
                    }
                }
            }.apply { isEnabled = radioService != null && (!tracksEnabled || trackService != null) })
            if (radioService == null || (tracksEnabled && trackService == null)) root.addView(text("Preparing local sync storage…", 13, muted).apply { setPadding(dp(4), dp(8), dp(4), 0) })
        }
        show(root)
    }

    /**
     * Android 15+ (target SDK 35/36) draws apps edge-to-edge, so pad the page by the
     * system bar insets and make it scrollable to keep every action reachable.
     */
    @Suppress("DEPRECATION")
    private fun show(root: LinearLayout) {
        val baseLeft = root.paddingLeft
        val baseTop = root.paddingTop
        val baseRight = root.paddingRight
        val baseBottom = root.paddingBottom
        val scroll = ScrollView(this).apply {
            setBackgroundColor(pageBackground)
            isFillViewport = true
            addView(root, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
        scroll.setOnApplyWindowInsetsListener { _, insets ->
            if (Build.VERSION.SDK_INT >= 30) {
                val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                root.setPadding(baseLeft + bars.left, baseTop + bars.top, baseRight + bars.right, baseBottom + bars.bottom)
            } else {
                root.setPadding(baseLeft + insets.systemWindowInsetLeft, baseTop + insets.systemWindowInsetTop, baseRight + insets.systemWindowInsetRight, baseBottom + insets.systemWindowInsetBottom)
            }
            insets
        }
        setContentView(scroll)
        scroll.requestApplyInsets()
    }

    private fun confirmDeleteAccount() {
        if (!CloudSyncClient.configured()) {
            Toast.makeText(this, "Account deletion needs the cloud API • use meshcrossway.renbrant.com/delete-account", Toast.LENGTH_LONG).show()
            return
        }
        AlertDialog.Builder(this)
            .setTitle("Delete account?")
            .setMessage("This permanently deletes your MeshCrossway account and every cloud copy of your messages, favorites and GPX tracks. It cannot be undone.\n\nMessages, tracks, maps and settings stored on this phone are kept.")
            .setNegativeButton("Cancel", null)
            .setPositiveButton("Delete permanently") { _, _ ->
                Toast.makeText(this, "Deleting account…", Toast.LENGTH_SHORT).show()
                CloudSyncClient.deleteAccount(this) { message, success ->
                    runOnUiThread {
                        if (success) {
                            ClerkBridge.signOut(Runnable {
                                Toast.makeText(this, message, Toast.LENGTH_LONG).show()
                                render()
                            })
                        } else {
                            Toast.makeText(this, message, Toast.LENGTH_LONG).show()
                            render()
                        }
                    }
                }
            }
            .show()
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
