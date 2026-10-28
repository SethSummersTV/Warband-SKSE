# Warband: SKSE plugin source (WarbandUI.dll and RealmUI.dll)

This is the source code of the two SKSE plugins inside **Warband - Follower Framework - Party Manager** for Skyrim Special Edition, by Seth Summers:

- `WarbandUI.dll` (`src/`): the party screen, band HUD and the rest listed below.
- `RealmUI.dll` (`realmui/`): the "Your Estates" screen used by Warband's realm features.

Nexus Mods: https://www.nexusmods.com/skyrimspecialedition/mods/194934

## What it does

- The party screen, shown with Prisma UI.
- The band HUD, drawn with SKSE Menu Framework.
- Party stats read in C++, fighting styles, follower-line handling, members' own voices and follower systems, the portrait picker, and Skyrim Party Sheet's HUD settings.

Warband's Papyrus scripts ship with the mod itself (`Source\Scripts`).

## Built on

- [CommonLibSSE-NG by CharmedBaryon](https://github.com/CharmedBaryon/CommonLibSSE-NG), MIT licence: release 3.7.0 plus the next commit on `main` (b93280e, "Fix light-mod count"). It is included here as a git submodule in `extern/CommonLibSSE-NG`.
- spdlog (MIT).
- The published API headers of Prisma UI and SKSE Menu Framework, unchanged.

All third-party notices are in `THIRD_PARTY_NOTICES.txt` (WarbandUI) and `realmui/RealmUI_licenses.txt` (RealmUI), and ship next to each dll.

Game versions: Skyrim SE 1.5.97 and AE 1.6.x. Game details the library does not cover are in `src/game` and `realmui/game`. VR is not a target.

## Build

Windows, Visual Studio 2022 (MSVC) and xmake:

```
git clone --recurse-submodules https://github.com/SethSummersTV/Warband-SKSE.git
cd Warband-SKSE
xmake f -p windows -a x64 -m release --toolchain=msvc --skyrim_vr=n -y
xmake -y
```

The dlls are written to `build\windows\x64\release\` (`WarbandUI.dll`, `RealmUI.dll`).

## Licence

Copyright (c) 2026 Seth Summers. All rights reserved. See `LICENSE`.

The source is published so anyone can read and verify it. Third-party code keeps its own licence.
