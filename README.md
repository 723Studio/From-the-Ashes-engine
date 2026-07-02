# From the Ashes Engine

Public GPL engine source mirror for **X-Com: From the Ashes**.

This repository contains the open-source engine component used by From the Ashes releases. It is generated from the private development monorepo and published so that users can inspect, build, modify, and redistribute the corresponding engine source for released binaries under the applicable GPL terms.

## Repository status

This repository is a **generated source mirror**, not the canonical development repository.

Canonical development happens in the private `723Studio/From-the-Ashes-monorepo`. During the release process, the `engine/` subtree from that monorepo is exported, checked for forbidden/private files, committed here, and tagged with the same release tag used by the public binary release.

Direct development in this repository is intentionally limited. Pull requests and issues may still be useful as source suggestions or bug reports, but accepted code changes are applied to the private monorepo first and then mirrored back here during a release.

## What is included

This repository is intended to contain only the GPL-compatible engine source tree and closely related engine files.

It should not contain:

- proprietary From the Ashes content;
- encrypted official `.oxc` packages;
- private release configuration;
- encryption keys or other secrets;
- generated release artifacts;
- local Visual Studio user files such as `.vcxproj.user`.

## Releases and corresponding source

Public binary releases are published separately at:

https://github.com/723Studio/From-the-Ashes-releases

For every public binary release, the release page should provide both:

- a source archive attached to the binary release;
- a tag in this repository pointing to the corresponding public engine source snapshot.

Example release tag format:

- `v1.6.0-rc.1` for release candidate builds;
- `v1.6.0` for stable builds.

## Proprietary game content

From the Ashes is distributed as a standalone game product with proprietary official content. That content is not part of this source mirror.

Official From the Ashes content is distributed through encrypted `.oxc` packages in the public release artifacts. The presence of this engine source mirror does not make proprietary game content open-source.

## Relationship to OpenXcom / OpenXcom Extended

From the Ashes is based on a fork of OpenXcom Extended. This repository exists to publish the engine-side source corresponding to From the Ashes public binaries.

OpenXcom and OpenXcom Extended remain separate upstream projects.

## Reporting issues

For release testing, crashes, missing resources, packaging problems, and gameplay feedback, use the public release repository or Discord:

https://github.com/723Studio/From-the-Ashes-releases

https://discord.gg/PpyQ3NpYKf
