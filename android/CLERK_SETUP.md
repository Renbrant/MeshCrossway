# Clerk setup for MeshCrossway Android

The Android client now has the first Clerk integration slice: native SDK initialization, a small account shortcut in the app header, Clerk's hosted sign-in/sign-up page, session restoration, and sign-out. Sign-in methods are controlled in the Clerk dashboard, so Google and Apple can be offered without embedding provider secrets in the APK.

**This does not enable cloud backup yet.** The account screen intentionally says so, and the APK does not upload messages, locations, tracks, or settings. The Cloudflare Worker API must be deployed and the Android sync client still needs to be connected before cross-device data is available.

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
```

Then build from `android/`:

```sh
./gradlew :app:assembleDebug
```

Without the property, the app builds with account sign-in disabled and the local-first app continues to work. A publishable key is public by design, but using a local Gradle property keeps environment-specific keys out of commits. For GitHub's permanent-release workflow, add a repository Actions variable named `CLERK_PUBLISHABLE_KEY`. Production Google OAuth must also include the permanent Android release signing certificate fingerprint; the debug key fingerprint is separate.

## Current boundaries

- Clerk handles identity and session lifecycle; OAuth provider secrets remain in Clerk.
- The app uses Clerk's hosted Account Portal. The provider buttons shown there depend on the dashboard configuration.
- Sign-in does not opt a person into cloud storage. No content is uploaded by this integration slice.
- Before enabling sync, configure the Cloudflare Worker issuer to exactly match the Clerk session token issuer, deploy the Worker, and connect the client to the API. The existing Worker enforces the current free limits of seven days for ordinary messages, permanent favorites, and five GPX tracks up to 5 MiB each.
- Google native sign-in requires the Google/Clerk redirect and certificate setup. Apple sign-in requires the Apple provider connection to be configured in Clerk.

## References

- [Clerk Android quickstart](https://clerk.com/docs/android/getting-started/quickstart)
- [Clerk hosted authentication](https://clerk.com/docs/guides/account-portal/hosted-auth)
- [Clerk Android Google connection setup](https://clerk.com/docs/android/guides/configure/auth-strategies/sign-in-with-google)
