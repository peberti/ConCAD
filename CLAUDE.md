# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ConCAD is a fork of TinyCAD (https://www.tinycad.net), an open-source schematic-capture program for Windows. It is a classic MFC desktop application — Win32 (x86) only, Unicode, MFC linked dynamically, toolset `v142`.

## Build

- Open `TinyCad.sln` at the repo root in **Visual Studio 2019 or 2022 (Community edition is fine) with the MFC C++ component installed**. Build → Build Solution (Ctrl+Shift+B). There is no CMake / no command-line build flow checked in.
- Configurations: `Debug|Win32` and `Release|Win32`. There is no x64 config.
- Pre-build step: `src/gitbranch.bat` runs on every build and (re)writes `src/BuildId.h` with `GIT_BRANCH` and a fresh `BUILD_UUID`. `BuildId.h` is intentionally regenerated — do not commit hand edits to it.
- Installer: NSIS script at `installer/TinyCAD.nsi` (not invoked by msbuild; run NSIS separately after a Release build).
- Tests: there is **no test project / no automated test suite**. Verification is manual — load `.dsn` files from `examples/` and exercise the affected UI paths. `CHANGES.md` has a "Quick test plan" section that lists the smoke tests for the recent fork features.

## Vendored third-party code (do not refactor casually)

- `src/SQLite/` — SQLite3 amalgamation + `CppSQLite3U` wrapper. Used by `LibrarySQLite` for the SQLite-backed library format.
- `src/rapidxml-1.13/` — header-only XML parser used by `XMLReader.cpp`.

## Architecture: the parts that span multiple files

### Document model (MFC Doc/View, three-layered)

- **`CMultiSheetDoc`** (`MultiSheetDoc.h/.cpp`) — outer `CDocument` subclass. One per opened file. Owns a collection of "sheets". Two concrete subclasses:
  - **`CTinyCadMultiDoc`** — a normal schematic design (`.dsn`). Each sheet is a `CTinyCadDoc`.
  - **`CTinyCadMultiSymbolDoc`** — a library symbol being edited. Each sheet is a `CTinyCadSymbolDoc`.
- **`CTinyCadDoc`** (`TinyCadDoc.h/.cpp`, `Io.cpp`) — one sheet's worth of drawing objects, page setup, and `CDetails`. `CTinyCadSymbolDoc` and `CTinyCadHierarchicalDoc` extend it.
- **`CTinyCadView`** (`TinyCadView.h/.cpp`) — `CScrollView` that renders the active sheet and routes input. Most user-facing command IDs and accelerators land here first.

Anything that touches "all sheets in the open design" lives on `CMultiSheetDoc` / `CTinyCadMultiDoc`; per-sheet state lives on `CTinyCadDoc`. The CHANGES.md "Shared design details" feature is an example of code that has to fan out from one sheet to all the others on Apply.

### Drawing objects

- **`CDrawingObject`** (`DrawingObject.h`, `DrawingObject.cpp`) is the abstract base for everything you can place on a sheet — wires, buses, pins, symbol instances, junctions, labels, text, rulers, images, hierarchical-symbol references, cables, etc.
- The **`ObjType` enum** in `DrawingObject.h` is the canonical type tag, used both at runtime and as the serialization discriminator. **Adding a new object type means adding an `xFoo = NN` value here**, plus a factory entry in `Io.cpp` (XML) and possibly `Stream*.cpp` (legacy binary). Existing numeric values must never be reassigned — they appear in saved files.
- Each concrete object type has its own `Draw<Name>.cpp` (`DrawLine.cpp`, `DrawJunction.cpp`, `DrawPin.cpp`, `DrawMethod.cpp`, …). A *single* type often handles several `ObjType` values that share geometry — e.g., `DrawLine.cpp` handles `xWire`, `xBus`, `xLine`, `xCable`, `xNoConnect`, etc., dispatching on `xtype`.
- **`CDrawMethod`** (`Object.h`, `DrawMethod.cpp`) is the placed-symbol instance. Its `Paint` consults the connector / per-instance-color logic; that override is implemented via `CContext::SetForcedColor`.

### Drawing context

- **`CContext`** (`Context.h/.cpp`) wraps `CDC` for all rendering. Pen/brush/text-color selection goes through `SelectPen`, `SelectBrush`, `SetTextColor`. The "forced color" override (used by connector instances) is layered in here, so every primitive automatically honors it without each draw routine knowing.

### Serialization — two formats

- **XML `.dsn`** is the live format. Save path: `CTinyCadDoc::SaveXML` → `CXMLWriter`. Load path: `Io.cpp` factory dispatches XML tags (`<WIRE>`, `<CABLE>`, `<SYMBOL>`, `<DETAILS>`, …) to the right `CDrawingObject::LoadXML`. New tags should be **additive** and tolerate older files that do not contain them.
- **Legacy binary `CStream`** (`Stream.cpp`, `StreamFile.cpp`, `StreamMemory.cpp`) is still readable for old files but is **not extended**. New features (tokens, cables, connector color overrides — see CHANGES.md) exist only in XML.

### Libraries (symbol storage)

- **`CLibraryStore`** (`LibraryStore.h`) is the abstract symbol-library backend. Concrete: `CLibraryFile` (legacy binary `.lib`), `CLibraryDb` (Access/ODBC), `CLibrarySQLite` (SQLite — preferred). `CLibraryStoreNameSet` is a single named symbol record; `CSymbolRecord` (`Symbol.h`) is the per-orientation drawing payload.
- A "connector" flag lives on `CSymbolRecord` and serializes as a `<CONNECTOR>` element; placed instances (`CDrawMethod`) carry the optional per-instance `use_color` / `color` attributes when overridden.

### Netlist

- **`CNetList`** (`Net.h/.cpp`, `NetList.cpp`) walks the drawing to build nets. `xWire` and `xCable` are treated as electrically equivalent — both feed the same trace/junction/label-binding paths. If you add another wire-like object, mirror the existing `xWire || xCable` checks in `NetList.cpp`, `DragUtils.cpp`, `JunctionUtils.cpp`, and `TinyCadDoc.cpp` rather than inventing a new code path.

### Title block / "Design Details"

- **`CDetails`** (`Details.h/.cpp`) is per-sheet title-block state: Title, Author, Revision, DocNo, Organisation, Date, plus a `map<CString, CString>` of user-defined tokens. `Display(CContext&)` does token substitution at paint time (`{Title}`, `{Sheets}`, and any user-defined `{Name}`; built-ins are case-insensitive; cycle-safe up to 8 passes).
- **`CDetailsPropertySheet`** (`DetailsPropertySheet.*`) + `CDetailsPropertyPage1/2/3` are the `File → Design Details…` dialog (Design / Guides / Variables tabs). Page 1 and Page 3's `OnApply` propagate edits to every sheet via `CDetails::CopyDesignFields`.
- "Sheets X of Y" is recomputed into `CDetails::m_sSheets` immediately before every paint (`TinyCadDoc.cpp`) and every save (`Io.cpp`), so the in-memory value is never trusted.

## Resources / IDs

- `src/resource.h`, `src/TinyCad.rc` — Windows resource IDs and dialog templates. **Never reuse a numeric ID**; pick fresh ones for new commands / controls.
- `src/res/Toolbar.bmp`, `src/res/toolbar1.bmp` — drawing toolbars are 16×15-pixel sprite sheets. The "Drawing" toolbar (`IDR_DRAWING`) is currently full (15/15 slots used) — adding a new tool button means extending the bitmap and the toolbar resource. (For example, the Cable tool ships with only a keyboard shortcut for this reason; see CHANGES.md §4.)

## Fork-specific features

`CHANGES.md` at the repo root documents the features this fork adds over upstream TinyCAD: user-defined title-block tokens, automatic "Sheets X of Y", shared design fields across sheets, the Cable drawing tool (Shift+F2), and the connector-symbol-with-per-instance-color flow. Read it before touching `CDetails`, `DrawLine`/`xCable`, or the connector color-override code in `CContext` / `CDrawMethod` — it explains both the UX and the on-disk format choices.

## File-format compatibility rules

When you change anything that serializes:

- Emit new XML elements/attributes **only when the feature is in use** so older readers tolerate files saved by the new build.
- Never repurpose an `ObjType` enum value or an existing XML tag/attribute name.
- Do not extend the legacy binary `CStream` format. New state lives in XML only.
