<p align="center">
  <img src="art/concad-logo.png" alt="ConCAD logo" width="200">
</p>

# ConCAD

ConCAD is a schematic-capture program for Windows — a fork of
[TinyCAD](https://www.tinycad.net) focused on multi-sheet design
documentation: title-block tokens, shared design details, cables, and
PDF/SVG output.

It is a classic MFC desktop application: Win32 (x86) only, Unicode, MFC
linked dynamically, toolset `v142`.

## Features added over TinyCAD

This fork extends upstream TinyCAD with:

- **User-defined title-block tokens** — reference named variables with
  `{TokenName}` syntax in title-block fields, free text, and notes;
  edited under `Edit → Design Details…` (Ctrl+D).
- **Automatic "Sheets X of Y"** — the `{Sheets}` built-in token resolves
  to the current sheet's position in the design, recomputed on every
  paint and save.
- **Shared design details across sheets** — title-block fields and
  tokens propagate to every sheet of a design on Apply.
- **Cable tool** (Shift+F2) — a wire-like object electrically equivalent
  to a wire in the netlist.
- **Connector library type + per-instance color** — symbols flagged as
  connectors can carry a per-instance color override.
- **SVG title blocks** — pick an SVG title block per design.
- **Drawing defaults page** under `Options → Settings`.
- **Export as PDF** — export the active sheet to PDF.

See [CHANGES.md](CHANGES.md) for the full feature descriptions, on-disk
format notes, and a manual test plan.

## Building

Requires **Visual Studio 2019 or 2022** (Community edition is fine) with
the **v142 toolset and MFC for v142** installed. See [SETUP.md](SETUP.md)
for setting up a fresh machine.

1. Open `ConCad.sln` at the repo root.
2. Build → Build Solution (Ctrl+Shift+B).

Configurations are `Debug|Win32` and `Release|Win32` — there is no x64
build. A pre-build step (`src/gitbranch.bat`) regenerates
`src/BuildId.h` on every build; do not commit hand edits to it.

There is no CMake or command-line build flow, and no automated test
suite — verification is manual (load a `.con` design and exercise the
affected UI paths).

## Building the installer

The Windows installer is built with [NSIS](https://nsis.sourceforge.io)
(`makensis`), **separately from the Visual Studio build** — MSBuild does
not invoke it. The script is `installer/ConCAD.nsi`.

1. **Build the app** in Visual Studio with the **`Release | Win32`**
   configuration. Confirm `Release\ConCAD.exe`, `libpng16.dll`, and
   `zlib.dll` are produced.
2. **Provide the VC++ redistributable**: place `VC_redist.x86.exe` in
   `installer/` (download from
   <https://aka.ms/vs/17/release/vc_redist.x86.exe>). It is bundled and
   run silently by the installer, and is git-ignored.
3. **Install NSIS** (one-time). It ships the MUI2 library the script
   uses; the custom page (`AllUsersDlg.nsdinc`) is already in
   `installer/`.
4. **Compile from inside `installer/`** — the script uses relative paths
   (`..\Release\ConCAD.exe`) that only resolve when the working
   directory is `installer/`. Right-click `ConCAD.nsi` → *Compile NSIS
   Script*, or run:

   ```
   makensis ConCAD.nsi
   ```

The output is `installer\ConCAD_<version>_Production_Release.exe`, a
self-contained installer that installs ConCAD plus DLLs, the manual, and
the SVG title block, runs the VC++ redist, registers `.con` files, and
creates shortcuts. The version string is set by `PRODUCT_VERSION` at the
top of `ConCAD.nsi` — bump it per release. The installer does not bundle
symbol libraries; a fresh install starts empty and users add their own.

## Repository layout

| Path           | What it is                                              |
|----------------|--------------------------------------------------------|
| `src/`         | Application source (MFC Doc/View). Vendored SQLite and rapidxml under `src/SQLite/` and `src/rapidxml-1.13/`. |
| `templates/`   | Drawing templates.                                     |
| `manual/`      | User documentation.                                    |
| `installer/`   | NSIS installer script and supporting files.            |
| `art/`         | Logo / icon sources and the icon generator.            |
| `CHANGES.md`   | Fork feature documentation and test plan.              |
| `CLAUDE.md`    | Architecture notes and contribution guidance.          |

## License

ConCAD inherits TinyCAD's license: the **GNU Lesser General Public
License, version 2.1** (or, at your option, any later version). See
[LICENSE](LICENSE) for the full text, and the upstream project at
[www.tinycad.net](https://www.tinycad.net) and
[github.com/matt123p/TinyCAD](https://github.com/matt123p/TinyCAD).
