# True Third-Person Camera

A third-person camera for Baldur's Gate 3. One button switches to an over-the-shoulder view,
and pressing it again puts the camera back.

The pitch changes on its own between the two views: horizontal in the close view, and a
diagonal angle in the far view. During combat there is a dynamic action camera that
takes over when you cast a spell or when it is the enemy's turn.

Character size is tracked while you play and the camera offsets are adjusted to it
automatically, so wild shape forms and summons are handled as well.

The MCM menu is split into categories and every setting can be changed in real time. It also
has a preset picker with built-in camera setups, and presets made by other players can be
imported too.

This mod started as a fork of Native Camera Tweaks by Ershin, and a lot of that camera hook
code is still in use here, so thanks for all the hard work.

## Contents

The mod has two parts and you need both of them.

- `src/` is the native DLL. It hooks into the game's camera code.
- `script-extender/` is a Script Extender mod that gets packed into a .pak. It tells the DLL
  things it can't work out on its own, like wild shape forms, combat state and MCM settings.

The folders under `script-extender/Data/Mods/` are nested pretty deep, but that's just how BG3
wants them inside a .pak.

## Mouse-look toggle

In MCM, open **True Third-Person Camera > Global Settings > Mouse** and change
**Middle Mouse Look Toggle**. It is enabled by default and reloads without restarting.
Keep the game's Camera Rotate binding on middle mouse.

Click middle mouse once to keep mouse look active after releasing the button.
Click again to restore the cursor. Escape, focus loss, minimizing the game, or
controller input releases mouse look. Toggle back to the cursor before using menus.
Turning the setting off restores normal hold-to-look behavior.

Install both the updated DLL and companion `.pak` to see this checkbox.
The toggle supports WASD's first-event hook without replacing its cursor behavior.

## Building

Visual Studio 2022 with C++ support, CMake 3.21+, and vcpkg with `VCPKG_ROOT` set.

```powershell
cmake --preset REL -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The DLL lands in `build/Release/`. Copy it into `Baldurs Gate 3/bin/NativeMods/`.

## Packing the .pak

divine.exe comes from [LSLib](https://github.com/Norbyte/lslib):

```powershell
./pack.ps1 -DivinePath 'C:/path/to/ExportTool/tools/Divine.exe'
```

The script writes `dist/TrueThirdPersonCamera.pak`, extracts it again, and compares
every packaged file with its source. Install it with your mod manager as an update
to the existing companion mod. The DLL alone won't do anything on its own.

## Licence

GPL-3.0, inherited from Native Camera Tweaks. See COPYING and EXCEPTIONS.
