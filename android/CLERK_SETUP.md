# Clerk setup for MeshCrossway Android

The Android client now has the first Clerk integration slice: native SDK initialization, a small account shortcut in the app header, Clerk's hosted sign-in/sign-up page, session restoration, and sign-out. Sign-in methods are controlled in the Clerk dashboard, so Google and Apple can be offered without embedding provider secrets in the APK.

**Cloud sync is being brought online in stages.** The Android source now includes an opt-in, user-triggered sync flow for recent messages and favorite chats. It remains unavailable until the Cloudflare Worker is deployed, its Clerk issuer is configured, and `CLOUD_API_BASE_URL` is supplied to the build. Tracks are not yet synchronized. No upload occurs before the user signs in and enables sync.

## Clerk dashboard

1. Create a Clerk application for MeshCrossway.
2. In **Native applications**, enable the **Native API** and register namespace `com.meshcrossway.android` and Android package name `com.meshcrossway.mobile`.
3. In **SSO connections**, enable Google and Apple for both sign-in and sign-up. New social users are created on their first sign-in.
4. For Google, follow Clerk's Android social connection setup: create Android and Web OAuth clients, register the package name and signing certificate SHA-1, and put Google's Web client ID and secret in Clerk's dashboard. Do not place Google's secret in this repository or APK.
5. Copy the Clerk **Publishable Key** (`pk_test_…` for development or `pk_live_…` for production). Never use a Clerk Secret Key in the Android app.

Clerk notes that enabling Native API creates a public request path that bypasses browser CAPTCHA challenges. Review Clerk's Native API challenge guidance before inviting broad public use.

## Local Android build

Add the publishable key to the user-level Gradle properties file, outside the repository:

- Windows: `%USERPROFILE%\.gradle\gradle.properties`
- macOS/Linux: `~/.gradle/gradle.properties`

```properties
clerkPublishableKey=pk_test_replace_with_your_key
cloudApiBaseUrl=https://your-worker-hostname.workers.dev
```

Then build from `android/`:

```sh
./gradlew :app:assembleDebug
```

Without `clerkPublishableKey`, the app builds with account sign-in disabled. Without `cloudApiBaseUrl`, cloud sync stays unavailable and no data is sent. A Clerk publishable key and the HTTPS Worker URL are public configuration, not secrets; keep environment-specific values in local Gradle properties or GitHub repository variables. The APK workflows read `CLERK_PUBLISHABLE_KEY` and `CLOUD_API_BASE_URL`. Production Google OAuth must also include the permanent Android release signing certificate fingerprint; the debug key fingerprint is separate.

## Current boundaries

- Clerk handles identity and session lifecycle; OAuth provider secrets remain in Clerk.
- The app uses Clerk's hosted Account Portal. The provider buttons shown there depend on the dashboard configuration.
- Sign-in does not opt a person into cloud storage. The user must enable sync and tap **Sync now**; this first client slice uploads recent messages and favorite-chat history only. It does not upload Bluetooth radio identifiers, location, contacts, or radio settings.
- Configure `CLERK_ISSUER` in `cloud/wrangler.toml` to match the Clerk instance, deploy the Worker, and set the HTTPS Worker URL as `cloudApiBaseUrl` (local build) or the GitHub repository variable `CLOUD_API_BASE_URL` (Actions build). Apply outstanding D1 migrations before deployment. Development issuer and API keys must not be used for a public production launch.
- The Worker applies the free retention limits: ordinary messages are kept for seven days; favorite chats are retained until unfavorited. Its R2 API enforces up to five GPX tracks at 5 MiB each, but Android track synchronization is not yet connected.
- Google native sign-in requires the Google/Clerk redirect and certificate setup. Apple sign-in requires the Apple provider connection to be configured in Clerk.

## References

- [Clerk Android quickstart](https://clerk.com/docs/android/getting-started/quickstart)
- [Clerk hosted authentication](https://clerk.com/docs/guides/account-portal/hosted-auth)
- [Clerk Android Google connection setup](https://clerk.com/docs/android/guides/configure/auth-strategies/sign-in-with-google)
