# Licensing policy

Decision recorded: 2026-10-28. Original MeshCrossway code and documentation remain under [MIT](../LICENSE). This policy explains scope and review practices; it does not replace upstream licenses or add restrictions to MIT.

## Scope

| Material | Treatment |
|---|---|
| Original Core firmware, scripts and documentation | MIT, preserving copyright and permission notices |
| Combined display firmware | Original MIT code plus Waveshare and resolved third-party components under their own terms; do not describe the entire binary as MIT-only |
| Native Radio Pod firmware | Separate upstream licenses, including any modifications and redistribution obligations |
| Original archived KiCad designs | No license change in this decision; preserve existing grants and third-party library notices |
| Vendor CAD, photographs, maps, fonts and other assets | Their own terms; availability for download is not permission to redistribute |
| Project name and logo | No trademark registration or exclusive trademark rights are asserted by this policy; copyright licenses do not establish permission to imply project endorsement |

See the [dependency review](third-party-licenses.md), [notices](../THIRD_PARTY_NOTICES.md) and [image credits](images/README.md). A future CERN-OHL decision for original hardware must define scope and respect existing MIT grants; this change does not apply a new hardware license.

## Why Mesh-Client's change does not automatically require ours

[Mesh-Client's explanation](https://colorado-mesh.github.io/mesh-client/license/) says that it bundled GPL-3.0-only `@meshtastic/core` into its Electron renderer and changed its application license to GPL-3.0-or-later to address the inconsistency. Its Reticulum helper has separate AGPL terms. This is a dependency-compliance lesson, not evidence that all clients of Meshtastic must use GPL.

MeshCrossway's current design has a Core and two independently operating Radio Pods connected through BLE APIs. Interoperability alone does not automatically relicense the Core. However, separate devices or processes are not a blanket exemption: review code provenance, the nature of the integration and every component actually distributed.

In particular, review `.proto` definitions, generated bindings, copied examples and SDKs individually. A protocol description, a schema, a generator and its output need not share the same license. Do not assume generated code is unrestricted.

## Dependency and contribution review

Before incorporating or upgrading third-party material:

1. Record its source URL, exact revision/version, paths, license and copyright notices in the dependency review. Record whether it is incorporated, separately distributed, downloaded for a build, or research only.
2. Read the license at that revision, including exceptions, file-level notices, bundled libraries and transitive dependencies. An upstream repository's main license is not sufficient for all of its contents.
3. For GPL/AGPL material, assess whether the proposed distribution creates a combined work, and which precise license versions apply. MIT originals may remain MIT within a GPL-covered combination, but MIT alone cannot describe the whole distribution. Do not import GPL code into an MIT-only release without resolving this scope.
4. Preserve required notices, license texts and modification statements. For copyleft binaries, prepare corresponding source, build material and any applicable installation information according to the upstream license.
5. Record unresolved items explicitly. Do not label a release fully audited while they remain unresolved.

MIT, Apache and GPL permit commercial use. GPL does not require royalties or submission of changes to this repository. Its source obligations generally arise on distribution; private modifications are a different case. AGPL adds requirements for certain modified software used over a network. See the [GNU FAQ](https://www.gnu.org/licenses/gpl-faq.html.en).

## Release checks

- Record the MeshCrossway commit, Waveshare commit, toolchain version and exact resolved component versions for each newly built binary; retain `dependencies.lock`, component manifests and build configuration with the release evidence.
- Inspect built-in ESP-IDF components and binary libraries as well as Component Manager dependencies. Review the actual resolved graph, not only the direct dependency list.
- Supply the required third-party license texts, copyright notices and any upstream NOTICE files with the distribution. Add a modification notice for the patched Waveshare example. The files in `LICENSES/` are an initial collection, not a complete binary license bundle.
- Verify required source availability against the exact binary and revisions being distributed. A link to a moving default branch is not a reproducibility record.
- Review recovery images separately. Preserve hardware-tested binary bytes; adding documentation does not retroactively audit or rebuild those images.
- Check separately distributed Pod firmware, images, CAD, fonts and map data. Do not treat a firmware software license as permission for every asset.

Current outstanding release work is tracked in the [dependency review](third-party-licenses.md#open-review-items). These are manual review requirements; no automatic license scanner or complete SBOM is claimed.

## Future license decisions

Consider GPLv3 for original software if distributed derivatives should preserve source access or an incorporated dependency requires a GPL-covered combination. Check `-only` versus `-or-later` compatibility explicitly. Review AGPL only for a concrete network-service requirement.

Any future change must identify affected paths, release boundary, authorship and compatible third-party permissions. Preserve existing MIT notices and previously granted rights; new terms cannot withdraw permissions on copies already released under MIT.
