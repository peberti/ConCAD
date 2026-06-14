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
the **MFC C++ component** installed.

1. Open `TinyCad.sln` at the repo root.
2. Build → Build Solution (Ctrl+Shift+B).

Configurations are `Debug|Win32` and `Release|Win32` — there is no x64
build. A pre-build step (`src/gitbranch.bat`) regenerates
`src/BuildId.h` on every build; do not commit hand edits to it.

There is no CMake or command-line build flow, and no automated test
suite — verification is manual (load designs from `examples/` and
exercise the affected UI paths).

## Installer

An NSIS script lives at `installer/ConCAD.nsi`. It is **not** invoked by
MSBuild — run NSIS separately after a Release build.

## Repository layout

| Path           | What it is                                              |
|----------------|--------------------------------------------------------|
| `src/`         | Application source (MFC Doc/View). Vendored SQLite and rapidxml under `src/SQLite/` and `src/rapidxml-1.13/`. |
| `examples/`    | Sample designs, drawings, libraries, and VHDL.         |
| `templates/`   | Drawing templates.                                     |
| `manual/`      | User documentation.                                    |
| `installer/`   | NSIS installer script.                                 |
| `art/`         | Logo / icon sources and the icon generator.            |
| `CHANGES.md`   | Fork feature documentation and test plan.              |
| `CLAUDE.md`    | Architecture notes and contribution guidance.          |

## License

ConCAD inherits TinyCAD's license (GNU GPL). See the upstream project at
[www.tinycad.net](https://www.tinycad.net) and
[github.com/matt123p/TinyCAD](https://github.com/matt123p/TinyCAD).
