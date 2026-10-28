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
