# Contributing to MeshCrossway

Open an issue or pull request with the affected hardware revision, software version and reproducible details. Do not include private keys, network passwords or personal location histories. Distinguish real-device results from simulation.

## Licensing and provenance

Original contributions are accepted under the repository's [MIT license](LICENSE), unless explicitly identified as third-party material with its own compatible terms. Contribute only material you are entitled to provide. This policy does not require copyright assignment and does not relicense third-party code.

When adding or changing a dependency, copied code, generated binding, image, font, CAD model or dataset, include:

- Its source URL and exact version/commit, with affected paths.
- Its license, copyright notices, exceptions and required attribution.
- Whether it is linked/compiled, separately distributed, build-only, or a research reference.
- An update to the [dependency review](docs/third-party-licenses.md) and [third-party notices](THIRD_PARTY_NOTICES.md), including any unresolved questions.

Review GPL/AGPL source, `.proto` definitions and generated code before incorporation. Do not assume that an API, a downloaded example or generated output is MIT-compatible. Preserve existing upstream notices and mark modifications to upstream source.

Read the [licensing policy and release checks](docs/licensing.md) before distributing firmware. License review should follow dependency and build changes; passing compilation is not evidence of license compliance.

## Validation

Run checks relevant to the change. For firmware, use the existing preflight scripts and documented build workflow, record the exact source version and report physical tests separately. Documentation-only changes should preserve local links and accurately state review limitations.

## Android development and releases

Use `main` as the base and target of Android pull requests. It contains the app source alongside firmware, cloud code, documentation and the website. The historical `baseline/android-0.1.0` branch produced Android releases through 0.4.8; continue development on `main` after that integration.

Audit a shipped APK using its immutable `android-v<version>` tag, not a moving branch. Android 0.4.8 is tagged at `bf0f0c7fe74b3278edd0c134add80f32019bfe01` (versionCode 42). This integration preserves its Android build configuration, runtime code and tests; it does not republish the APK or move existing tags.

Before merging Android changes, require the Android unit/build/lint checks and API 29/35 storage instrumentation to pass. Report real-radio validation separately. Run `bash scripts/test_android_probe.sh`, `node scripts/test_android_remote_web.cjs` and `python3 scripts/test_android_migration.py` for the local fixture checks.

Publish subsequent versions by manually running **Android permanent-key release** on `main`, with a version matching `android/app/build.gradle` and a strictly increasing versionCode. Pushes and pull requests validate code without publishing. The release job accepts only `main`, rejects a superseded source commit or existing tag/release, and verifies the permanent signing identity. Keep existing tags and release assets unchanged; install compatible updates without uninstalling to retain app data.
