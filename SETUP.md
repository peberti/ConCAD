# Setting up a ConCAD build machine

How to go from a freshly installed Windows machine to a working ConCAD build.
See [README.md](README.md) for the build and installer steps themselves.

## Before you start: do not lose `packages/`

`packages/` (vendored NuGet native packages) and `installer/VC_redist.x86.exe`
are **git-ignored**, so a fresh `git clone` does not bring them back.

- `packages/` holds the exact `Win32/v142` libraries the link needs
  (`libjpeg_static`, `libiconv`, `libpng`, `zlib`, …). These are old packages
  on unreliable feeds, so restoring them can be slow or fail. Back this folder
  up together with the repo.
- `VC_redist.x86.exe` can be downloaded again from
  <https://aka.ms/vs/17/release/vc_redist.x86.exe>.

## 1. Git for Windows

```
winget install --id Git.Git -e
```

The pre-build step `src/gitbranch.bat` also finds the `git.exe` bundled with
Visual Studio, so builds still work without this. You still need it for normal
Git work on the Windows side.

## 2. Visual Studio 2022 Community, with the v142 toolset and MFC

The project is pinned to the **v142** toolset (VS 2019 compiler), and it
**must stay on v142**: `libiconv` ships only v142 libraries for Win32, and
`libjpeg_static` has nothing newer than v142. Building with VS 2022's default
v143 fails at link time with unresolved `_jpeg_*` / `_libiconv_*` symbols.

One-shot install from an elevated Windows Terminal:

```
winget install --id Microsoft.VisualStudio.2022.Community -e --override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --add Microsoft.VisualStudio.ComponentGroup.VC.Tools.142.x86.x64 --add Microsoft.VisualStudio.Component.VC.14.29.16.11.MFC"
```

| Component ID | What it is |
|---|---|
| `Microsoft.VisualStudio.Workload.NativeDesktop` | Desktop development with C++ |
| `Microsoft.VisualStudio.ComponentGroup.VC.Tools.142.x86.x64` | MSVC v142 – VS 2019 C++ build tools (v14.29-16.11) |
| `Microsoft.VisualStudio.Component.VC.14.29.16.11.MFC` | C++ v14.29 (16.11) MFC for v142 build tools (x86 & x64) |

If you use the installer GUI instead, select the workload, then under
*Individual components* search for `v142` and tick both items above.
**Trap:** *ATL for v142* is not *MFC for v142*. If you pick ATL, the build
fails on `afxwin.h`.

## 3. NSIS (only for building the installer)

```
winget install --id NSIS.NSIS -e
```

## 4. First build

1. Open `ConCad.sln` at the repo root.
2. If Visual Studio offers to **retarget** the project to a newer toolset or
   SDK, **decline**. Retargeting rewrites `PlatformToolset` to v143 or later,
   and then the link fails.
3. Select `Debug | Win32` and run Build → Build Solution.
4. Sanity check: the splash screen should show `Version 0.<N>` with `N > 0`.
   If it shows `0.0`, `src/gitbranch.bat` could not find git.

The `.vs/` folder is a per-user Visual Studio cache. Delete it if Visual
Studio behaves strangely after a reinstall.

## Optional: WSL

To use git from WSL against the same checkout:

```
sudo apt update && sudo apt install git
```
