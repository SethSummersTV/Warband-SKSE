# Warband: SKSE plugin source (WarbandUI.dll and RealmUI.dll)

This is the source code of the two SKSE plugins inside **Warband - Follower Framework - Party Manager** for Skyrim Special Edition, by Seth Summers:

- `WarbandUI.dll` (`src/`): the party screen, band HUD and the rest listed below.
- `RealmUI.dll` (`realmui/`): the "Your Estates" screen used by Warband's realm features, shown with Prisma UI.

Nexus Mods: https://www.nexusmods.com/skyrimspecialedition/mods/194934

## What it does

- The party screen: Prisma UI if it is installed, otherwise Magelight UI.
- The band HUD, drawn with SKSE Menu Framework.
- Party stats read in C++, fighting styles, follower-line handling, members' own voices and follower systems, the portrait picker, and Skyrim Party Sheet's HUD settings.

Warband's Papyrus scripts ship with the mod itself (`Source\Scripts`).

## Built on

- [CommonLibSSE NG, MinLL/CommonLibVR](https://github.com/MinLL/CommonLibVR), branch `ng`, release 4.39.5 (commit 550cc4f), MIT licence. This is an MIT continuation of CommonLibSSE NG, branched from the last MIT release. It is included here as a git submodule in `extern/CommonLibVR`.
- spdlog (MIT).
- The published API headers of Magelight UI, Prisma UI and SKSE Menu Framework, unchanged.

All third-party notices are in `THIRD_PARTY_NOTICES.txt` and ship next to the dll.

Game versions follow the library: Skyrim SE 1.5.97, AE 1.6.x (including 1.6.1130 and later) and 1.7.x. VR is not a target.

## Build

Windows, Visual Studio 2022 (MSVC) and xmake 3.x:

```
git clone --recurse-submodules https://github.com/SethSummersTV/Warband-SKSE.git
cd Warband-SKSE
xmake f -p windows -a x64 -m release --toolchain=msvc --skyrim_vr=n -y
xmake -y
```

The dlls are written to `build\windows\x64\release\` (`WarbandUI.dll`, `RealmUI.dll`). Each one ships with its notices file (`THIRD_PARTY_NOTICES.txt`, `realmui/RealmUI_licenses.txt`).

## Licence

Copyright (c) 2026 Seth Summers. All rights reserved. See `LICENSE`.

The source is published so anyone can read and verify it. Third-party code keeps its own licence.
