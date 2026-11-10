package com.meshcrossway.android

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast

class ClerkAccountActivity : Activity() {
    private val pageBackground = Color.rgb(9, 12, 16)
    private val panel = Color.rgb(24, 29, 36)
    private val orange = Color.rgb(255, 138, 36)
    private val white = Color.rgb(242, 244, 247)
    private val muted = Color.rgb(154, 166, 180)

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        window.statusBarColor = pageBackground
        window.navigationBarColor = pageBackground
        render()
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
        root.addView(cardText("Cloud synchronization is not connected yet. Signing in does not upload conversations, location, tracks, contacts, or settings."))
        root.addView(text("When sync is enabled, the free limits are planned as seven days for ordinary messages, permanent favorites, and up to five tracks of 5 MiB each.", 13, muted).apply {
            setPadding(dp(4), dp(12), dp(4), 0)
        })
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
