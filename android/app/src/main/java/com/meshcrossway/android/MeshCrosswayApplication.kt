package com.meshcrossway.android

import android.app.Application
import android.app.Activity
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.RippleDrawable
import android.content.res.ColorStateList
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.widget.FrameLayout
import com.clerk.api.Clerk

class MeshCrosswayApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        val key = BuildConfig.CLERK_PUBLISHABLE_KEY
        if (key.isNotBlank()) {
            Clerk.initialize(this, publishableKey = key)
        }
        registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityResumed(activity: Activity) {
                if (activity is MainActivity) addAccountShortcut(activity)
            }
            override fun onActivityCreated(activity: Activity, state: Bundle?) = Unit
            override fun onActivityStarted(activity: Activity) = Unit
            override fun onActivityPaused(activity: Activity) = Unit
            override fun onActivityStopped(activity: Activity) = Unit
            override fun onActivitySaveInstanceState(activity: Activity, state: Bundle) = Unit
            override fun onActivityDestroyed(activity: Activity) = Unit
        })
    }

    private fun addAccountShortcut(activity: Activity) {
        val content = activity.findViewById<FrameLayout>(android.R.id.content) ?: return
        if (content.findViewWithTag<View>("meshcrossway-clerk-account") != null) return
        val density = activity.resources.displayMetrics.density
        fun dp(value: Int) = (value * density + 0.5f).toInt()
        val button = FrameLayout(activity).apply {
            tag = "meshcrossway-clerk-account"
            contentDescription = "Account and sync"
            isClickable = true
            isFocusable = true
            background = RippleDrawable(
                ColorStateList.valueOf(0x44ffffff),
                ColorDrawable(0x22101010),
                ColorDrawable(Color.WHITE)
            )
            setOnClickListener { activity.startActivity(android.content.Intent(activity, ClerkAccountActivity::class.java)) }
        }
        button.addView(NavIcon(activity, "account", 0xFFFF8A24.toInt()), FrameLayout.LayoutParams(dp(24), dp(24), Gravity.CENTER))
        val params = FrameLayout.LayoutParams(dp(44), dp(44), Gravity.TOP or Gravity.END).apply {
            val statusBarId = activity.resources.getIdentifier("status_bar_height", "dimen", "android")
            val topInset = if (statusBarId > 0) activity.resources.getDimensionPixelSize(statusBarId) else 0
            topMargin = topInset + dp(5)
            marginEnd = dp(48)
        }
        content.addView(button, params)
    }
}
