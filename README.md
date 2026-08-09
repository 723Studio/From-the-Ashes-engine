# X-Com: From the Ashes — GPL executable source

This repository is the GPL source mirror for the executable component shipped as part of **X-Com: From the Ashes**. It does not have an independently maintained product version. Tags correspond to complete FTA releases.

The complete game is developed in the private `723Studio/From-the-Ashes-monorepo` source-of-truth repository. The public `723Studio/From-the-Ashes-engine` repository is an automated one-way mirror of its GPL-covered `engine/` subtree.

## Versioning

FTA has one project version shared by the executable, required base content package, release tags, manifests, and binary artifacts. The current source declares **Alpha 2 — 0.2.0** in `src/version.h`.

Public source tags always use `vMAJOR.MINOR.PATCH`, for example `v0.2.0`. Release maturity is represented only by GitHub's **Pre-release** or **Latest** designation on the matching binary release; it is not encoded in source tags or artifact names.

Builds also contain a generated diagnostic identifier in `YYYYMMDD-<commit>` form. This build ID identifies the source revision and UTC build date; it is not a separate engine or content version.

## Mirror scope

This mirror contains the GPL-covered C++/SDL executable source and its build files. It does not contain proprietary FTA content, encrypted official packages, private assets, encryption keys, release credentials, or original UFO/X-COM resources.

The source tree alone is therefore not a complete playable FTA distribution. Building without the private production content key does not grant access to the official encrypted content packages.

## Building

Supported project entry points include:

- `CMakeLists.txt` for CMake builds on supported platforms;
- `src/OpenXcom.2010.sln` for native Visual Studio builds on Windows.

The executable depends on SDL 1.2, SDL_image, SDL_mixer, SDL_gfx, and the other libraries referenced by the project files. Native Visual Studio and CMake builds generate the build ID automatically from the current Git commit and UTC build date.

## License and upstream

The executable source is distributed under the GNU General Public License, version 3. See [LICENSE.txt](LICENSE.txt).

FTA is built from a fork of [OpenXcom](https://github.com/SupSuper/OpenXcom) and [OpenXcom Extended](https://github.com/MeridianOXC/OpenXcom). Their projects remain valuable upstream technical references, but FTA product requirements and release versions are maintained by the FTA project.

Original X-COM/UFO Defense resources remain the property of their respective owners and are not included here. This project does not support piracy or redistribution of those resources.

## Contributions and mirror policy

Do not make authoritative changes directly in the public mirror. Engine development happens in the private monorepo, and the release pipeline publishes the matching GPL source snapshot for each complete FTA release.
