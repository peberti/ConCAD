# ConCAD changes

This document describes the features added in this round of development,
how they work, and how to test them. All changes are in the `src/`
directory of the ConCAD project. The XML `.dsn` file format is the only
serialization format affected; the legacy binary format is unchanged.

Build with Visual Studio (Community 2019/2022 with the MFC component
installed). Open `ConCad.sln` at the repo root, then Build → Build
Solution (Ctrl+Shift+B).

---

## Release 3.01.00 (2026-06-14)

First numbered ConCAD release. It collects the schematic-documentation
features this fork adds over upstream TinyCAD, plus SQLite-library
robustness fixes and packaging changes. Each item below is documented in
full in the correspondingly numbered section.

**Schematic / title-block features**

- **User-defined title-block tokens** — `{TokenName}` substitution in
  title-block fields, free Text, and Note objects, edited under
  `Edit → Design Details…` (Ctrl+D). (§1)
- **Automatic "Sheets X of Y"** — the `{Sheets}` built-in token,
  recomputed on every paint and save. (§2)
- **Shared design details + tokens across all sheets** — title-block
  fields and tokens propagate to every sheet on Apply. (§3)
- **Cable tool** (Shift+F2) — a wire-like object, electrically
  equivalent to a wire in the netlist. (§4)
- **Connector library type + per-instance color** — symbols flagged as
  connectors can carry a per-instance color override. (§5)
- **SVG title blocks** — pick an SVG title block per design, with bundled
  templates. (§7)
- **`Options → Settings` "Drawing" defaults page**. (§8)
- **Export as PDF** — export the active sheet to PDF, with toolbar
  button. (§9)

**Robustness**

- **SQLite library open/delete robustness** — no longer crashes or
  aborts when opening read-only or deleting SQLite-backed libraries. (§6)

**Packaging**

- Versioning unified at **3.01.00**: installer `PRODUCT_VERSION` and the
  EXE resource version (`3.1.0.0`) now match.
- Installer renamed to `installer/ConCAD.nsi`; it no longer bundles
  symbol libraries or example circuits — a fresh install starts with no
  libraries.

All schematic changes affect the XML `.dsn` format only; the legacy
binary format is unchanged, and older `.dsn` files still load.

---

## 1. Design Details: user-defined tokens

You can use named text variables (tokens) and reference them with
`{TokenName}` syntax. Tokens are substituted at render time in the
title-block fields, the SVG title block, **free Text objects, and Note
objects**. They are deliberately **not** substituted in electrical
labels or pin references, so the netlist is unaffected.

### Where it lives in the UI

`Edit → Design Details…` (Ctrl+D — moved from the File menu) has these
tabs:

- **Design** — the title-block fields (Title, Author, Revision,
  Document, Organisation, Date), the visibility checkbox, **and the
  Variables editor** (folded in from what used to be a separate tab).
- **Guides** — the rulers configuration.
- **Title Block** — the SVG title-block picker (see §7).

The Variables editor on the Design tab lists each referenced token as a
read-only **Name** label with an editable **Value** field (a scrollbar
appears when there are more tokens than fit). There is no Add/Remove —
a token appears automatically as soon as you type `{TokenName}`
somewhere it is substituted, and drops off when no longer referenced.

### How to use

1. Type `{ProjectCode}` into any title-block field, free Text, or Note.
2. Open `Edit → Design Details…` (Design tab). `ProjectCode` is listed
   under Variables; type its value (e.g. `PRJ-001`) and click OK.
3. Everywhere `{ProjectCode}` appears now renders `PRJ-001`.

Token names use letters, digits and underscores and cannot start with a
digit; reserved built-in names (below) are rejected.

### Built-in tokens

The following names work without defining anything (case-insensitive):

| Token            | Resolves to                |
|------------------|----------------------------|
| `{Title}`        | the Title field            |
| `{Author}`       | the Author field           |
| `{Revision}`     | the Revision field         |
| `{DocNo}`        | the Document Number field  |
| `{Document}`     | (alias of `{DocNo}`)       |
| `{Organisation}` | the Organisation field     |
| `{Org}`          | (alias of `{Organisation}`)|
| `{Sheets}`       | the auto "N of M" string   |
| `{Date}`         | the Date field             |

User-defined tokens override built-ins if you reuse a name (we block
exact-name collisions — the Add dialog refuses reserved names like
`Title`, `Author`, etc.).

### Notes

- Tokens can reference other tokens. Resolution runs up to 8 passes,
  which catches cycles safely. A token referenced only through another
  token's value still counts as "used" and stays listed.
- Unknown tokens are left as-is (literal `{XYZ}`).
- Tokens are stored inside each sheet's `<DETAILS>` block as
  `<USERTOKEN name="...">value</USERTOKEN>` entries and are shared across
  all sheets. Old `.dsn` files without tokens still load. Tokens that are
  no longer referenced anywhere are pruned on the next save.

### Files changed

- `src/Details.h`, `src/Details.cpp` (`GetTitleBlockSvg`, `Resolve`
  fast-path)
- `src/DetailsPropertyPages.h`, `src/DetailsPropertyPages.cpp` (Variables
  editor embedded on the Design page; reference discovery across all
  sheets' Text/Note objects; old `IDD_DETAILS_PAGE3` "Variables" tab no
  longer shown)
- `src/DetailsPropertySheet.cpp` (drops the separate Variables page)
- `src/DrawText.cpp`, `src/DrawNoteText.cpp`, `src/Object.h` (token
  substitution in free Text / Note objects)
- `src/ConCad.rc` (Design page hosts the Variables editor; Design
  Details moved to the Edit menu), `src/resource.h` (new control IDs)

---

## 2. Automatic "Sheets X of Y"

The Sheets cell of the title block is now computed automatically from
the current sheet's position within the multi-sheet design.

### Where it shows up

- **Title block (on the page)**: rendered as `1 of 3`, `2 of 3`, etc.
  for each sheet.
- **`File → Design Details…` → Design tab**: the Sheets field is
  labeled **"Sheets (auto)"** and shown read-only / grayed-out with
  the current value for reference.

### How it works

Before each paint and each save, the document computes:

- `current sheet index + 1` (1-based)
- `total sheet count`

and writes `<current> of <total>` into `CDetails::m_sSheets`. The same
value is what the `{Sheets}` built-in token resolves to.

### Behavior

- Add a new sheet → all sheets re-render with the updated total.
- Delete a sheet → same.
- Reorder sheets → the per-sheet number follows the new order.
- The value persists on disk (so a saved `.dsn` always has a sensible
  Sheets string), but the in-memory value is recomputed on every paint
  or save, so stale values are never displayed.

### Files changed

- `src/Details.h`, `src/Details.cpp` (new `SetSheetContext`,
  `GetSheetsDisplay`)
- `src/ConCadDoc.cpp` (compute index/total before
  `GetDetails().Display`)
- `src/Io.cpp` (compute index/total before `WriteXML`)
- `src/DetailsPropertyPages.cpp` (read-only field, no longer written
  on Apply)
- `src/ConCad.rc` (label change, `ES_READONLY` on the edit field)

---

## 3. Shared design details + tokens across all sheets

Design-level fields and user tokens are now mirrored across every
sheet in a multi-sheet design.

### What is shared

- Title
- Author
- Revision
- Document Number
- Organisation
- Date
- User tokens (the Variables tab)
- Title-block visibility checkbox
- Title-block SVG template selection

### What is **not** shared (still per-sheet)

- Page size
- Ruler / guides configuration
- Sheets X of Y (auto-computed per sheet)

### How the sync happens

When you click OK on `File → Design Details…`:

- The **Design** tab's OnApply writes the fields to the current sheet
  and then copies them to every other sheet.
- The **Variables** tab's OnApply writes the token map to every sheet
  (only if you actually touched the list — undirty pages don't trigger
  the propagation).

When you **add a new sheet**, the existing copy-on-add logic in
`CConCadMultiDoc` already initializes the new sheet's `CDetails` from
the current sheet's. So tokens and design fields are inherited.

### Files changed

- `src/Details.h`, `src/Details.cpp` (new `CopyDesignFields`)
- `src/DetailsPropertyPages.cpp` (propagation loops in OnApply for
  pages 1 and 3)

---

## 4. Cable tool

A new drawing tool that produces an electrically wire-equivalent line
that renders thicker. Useful for distinguishing cables/harnesses from
internal signal wires in a schematic.

### Where it lives in the UI

- **Keyboard shortcut: Shift+F2** (mirrors F2 for Wire). After
  pressing, click two points to place a cable segment. Right-click or
  Esc to exit the tool.
- **Toolbar button: not added** — the existing `IDR_DRAWING` toolbar
  bitmap is fully populated (15 of 15 slots used). To add a button
  later, open `res/toolbar1.bmp` in Visual Studio's resource editor,
  add a new 16×15 icon, then append `BUTTON IDM_TOOLCABLE` to the
  `IDR_DRAWING` toolbar in the .rc file. The command handler is
  already wired.

### Behavior

- Renders as a 3-pixel solid line (vs. 1 px for wire, 5 px for bus).
  Color is the same as Wire (in the Colours dialog).
- Snaps to component pins exactly like a wire.
- Forms junctions and splits like a wire when crossed.
- Drag/move of attached symbols brings the cable along.
- **Electrically a wire** — the netlist treats cables as part of the
  same net as wires they connect to. Two pins connected by a cable
  end up on one net, identical to connecting them with a wire.
- Serialized as `<CABLE a="..." b="..."/>` (vs. `<WIRE …/>`) so the
  type is preserved on round-trip and can be filtered separately
  later (e.g., for a wiring-table export).

### How to use

1. Press **Shift+F2** to activate the Cable tool.
2. Click the first endpoint (snaps to the nearest pin if you're near
   one).
3. Click the second endpoint.
4. Keep clicking to chain cable segments, or right-click/Esc to exit.

### Files changed

- `src/DrawingObject.h` (new `xCable = 143` enum value)
- `src/DrawLine.cpp` (constructor, GetXMLTag, GetName, getMenuID,
  Paint, SaveXML, LoadXML, snap/stick behavior all handle xCable)
- `src/Io.cpp` (factory dispatch on `<CABLE>` tag)
- `src/NetList.cpp` (cables routed through xWire net-tracing,
  junction-crossing, and label-binding logic)
- `src/DragUtils.cpp`, `src/JunctionUtils.cpp`, `src/ConCadDoc.cpp`
  (drag/junction/snap paths include xCable alongside xWire)
- `src/ConCadView.h`, `src/ConCadView.cpp` (new `OnSelectCable`
  handler, message-map entry)
- `src/resource.h` (new `IDM_TOOLCABLE = 32908`)
- `src/ConCad.rc` (Shift+F2 accelerator, status-bar string)

---

## 5. Connector library type + per-instance color

You can now mark a library symbol as a **connector**. Connector
instances on the schematic get an editable color override — click the
instance, pick a color, and just that instance renders in your chosen
color (other instances of the same library symbol are unaffected).

### Marking a library symbol as a connector

1. Open the Library that contains the symbol.
2. Edit the symbol (the symbol editor opens in a separate window).
3. Save/Store the symbol — the "Update Library Symbol" dialog
   (`IDD_UPDATE`) opens.
4. Tick **"This symbol is a connector (click on placed instances to
   change color)"** near the Description field.
5. Click **Store**.

### Recoloring a placed connector instance

1. Place the connector symbol on a schematic as usual.
2. Double-click the placed instance — the symbol-edit dialog opens.
3. The **Color…** button (top right of the dialog) is enabled because
   the library symbol is marked as a connector.
4. Click **Color…** → standard Windows color picker → OK.
5. The instance re-renders in the picked color.

### Behavior

- The Color button is **disabled** for non-connector symbols.
- Each placed instance has its own color — picking a color on one
  instance does not affect others.
- The override is stored on the placed instance, not on the library
  symbol, so the library can stay neutral while individual instances
  can be tinted (e.g., to distinguish power vs. ground vs. signal
  connectors of the same library part).
- Text labels (Ref, Name fields) on the instance are **not** tinted —
  they stay in the standard pin color. (If you want labels tinted too,
  it's a one-line change.)
- Serialized as `use_color="1" color="<integer>"` attributes on the
  placed `<SYMBOL>` tag in the .dsn file, only when an override is
  active.

### Reverting to the default color

There's currently no "remove override" button in the UI. To remove a
per-instance color override, you'd need to either:

- Delete and replace the symbol instance, or
- Edit the .dsn manually and remove the `use_color`/`color`
  attributes from the relevant `<SYMBOL>` tag.

If you want a "Default color" button added to the edit dialog, ask.

### Files changed

- `src/Symbol.h`, `src/Symbol.cpp` (`is_connector` field on
  `CSymbolRecord`, `<CONNECTOR>` XML tag)
- `src/DlgUpdateBox.h`, `src/DlgUpdateBox.cpp` ("Is connector"
  checkbox in `IDD_UPDATE`)
- `src/Object.h` (new fields and accessors on `CDrawMethod`)
- `src/DrawMethod.cpp` (constructor init, SaveXML, LoadXML,
  `IsConnector()`, color override in `Paint`)
- `src/Context.h`, `src/Context.cpp` (new `SetForcedColor`,
  `m_force_color`/`m_forced_color`, hook in `SelectPen`,
  `SelectBrush`, `SetTextColor`)
- `src/EditDlgMethodEdit.h`, `src/EditDlgMethodEdit.cpp` (new
  `OnPickColor` handler, Color button enable/disable logic)
- `src/ConCad.rc` (new "Co&lor…" button in `IDD_METHOD`, new
  "This symbol is a connector…" checkbox in `IDD_UPDATE`)
- `src/resource.h` (new `METHODBOX_COLOR = 40012`,
  `IDC_IS_CONNECTOR = 40011`)

---

## 6. SQLite library open/delete robustness

Fixes to the SQLite library backend (`CLibrarySQLite`, `CppSQLite3U`),
surfaced when bringing up the build on a clean machine with many
configured libraries — including read-only ones under `Program Files`.

- **Startup no longer crashes** on a library that fails to open. The
  catch handler in `CLibrarySQLite::Attach` called `close()`, which can
  itself throw (`SQLITE_BUSY`); that throw escaped the handler and went
  unhandled. It is now guarded.
- **The `IsConnector` migration (§5) is best-effort.** It probes with
  `PRAGMA table_info` and only runs `ALTER TABLE … ADD COLUMN` when the
  column is absent, inside a try/catch. A read-only or locked database
  (e.g. a library under `Program Files`) now loads normally instead of
  failing the whole library — the column read is skipped when the column
  is not present (`getIntField` throws on an unknown column rather than
  returning its default).
- **Deleting a library no longer aborts.** `~CppSQLite3DB` now wraps its
  `close()` in try/catch so a throwing `close()` cannot escape the
  destructor (which would call `std::terminate`/`abort` — the "abort()
  has been called" dialog). This matches the existing
  `~CppSQLite3Query` / `~CppSQLite3Statement` destructors.

### Build note: Visual Studio toolset

The solution targets PlatformToolset **v142** (Win32/x86). This is
required, not incidental: the vendored NuGet native libs
`libjpeg_static` and `libiconv.lib` (`packages/`) ship prebuilt `.lib`
files only for v140/v141/v142, so building with a newer toolset fails to
link (`_jpeg_*` / `_libiconv_*` unresolved externals). Install **C++ MFC
for the v142 build tools (x86 & x64)** in the Visual Studio installer
(an ATL-only install is not enough). Do not let Visual Studio
auto-retarget the project to a newer toolset.

### Files changed

- `src/LibrarySQLite.cpp`
- `src/SQLite/CppSQLite3U.cpp`

---

## 7. SVG title blocks

The title block can be drawn from an **SVG template** instead of the
built-in procedural box. `Edit → Design Details… → Title Block` lists
templates found in:

- `<exe-dir>/templates/title-blocks/` (installer-bundled),
- `<exe-dir>/../templates/title-blocks/` (dev-build fallback),
- `%APPDATA%/ConCAD/templates/title-blocks/` (per-user; files here get a
  ` (user)` suffix in the list).

Pick a template, **Browse SVG…** for an arbitrary file, or **Use
built-in** to revert. `{Token}` references inside the SVG (including the
built-ins and the user variables of §1) are substituted at paint time.

### On-disk format (hybrid, additive)

```xml
<TITLEBLOCK_SVG name="Simple-A4" enc="base64">PHN2Zy…</TITLEBLOCK_SVG>
```

- `name` — the template stem; re-resolved against the store on load so a
  re-shipped/edited bundled template propagates. Absent for Browse… files.
- `enc="base64"` — child data is base64 of the SVG's UTF-8 bytes. Legacy
  files with no `enc` are read as raw and upgraded to base64 on next save.
- If the named template is missing on a machine, the embedded copy is
  used — so designs shared via SharePoint render everywhere.

### Title block for new designs (File → New)

`File → New` (Ctrl+N) first asks which title block the new design should
use: **(Built-in title block)** or any template from the store. The choice
is remembered (`HKCU\Software\ConCAD\ConCAD\1x20\NewTitleBlock`) and
preselected next time; it is also applied silently to the blank design
opened at startup. Cancel aborts the New.

Tick **Don't ask again** to skip the dialog and always use the remembered
choice; turn asking back on with `Options → Settings → Drawing → Ask for a
title block on File > New` (`AskNewTitleBlock`). If the remembered
template is no longer installed, the new design gets the built-in title
block. No file-format change — the choice is stored in the design exactly
as if it had been picked on the Title Block tab.

### Files changed

- `src/SvgTitleBlock.{h,cpp}` (NanoSVG-based renderer + template store),
  `src/nanosvg/nanosvg.h` (vendored)
- `src/Details.{h,cpp}` (`m_sTitleBlockName` / embedded copy /
  `m_sEffectiveSvg`, `ResolveTitleBlock`, `DisplayBox` SVG branch)
- `src/DetailsPropertyPages.*`, `src/ConCad.rc` (the Title Block tab and
  the File → New picker `CPickTitleTemplateDlg` / `IDD_PICK_TITLE_TEMPLATE`)
- `src/ConCad.{h,cpp}` (`OnFileNewDesign`), `src/ConCadMultiDoc.cpp`
  (applies the remembered template in `OnNewDocument`),
  `src/ConCadRegistry.*`, `src/OptionsSheets.*` (re-enable checkbox)

---

## 8. Options → Settings: "Drawing" defaults page

A new **Drawing** tab in `Options → Settings` collects drawing defaults
(stored in the registry, applied to the current document immediately):

- **Wire width** / **Cable width** (px) — applied live to all wires /
  cables.
- **Wire colour** / **Cable colour** — colour pickers. Cable now has its
  own colour (`COLOR_CABLE`) instead of sharing the wire colour; wire
  colour stays in sync with the `Options → Colours` dialog.
- **New note boxes**: default **Background fill** and **Rounded corners**
  for newly created Note objects.
- **Components → Label font…** — face / weight / italic for component
  reference/value labels (height kept at the pin-font scale).

### Files changed

- `src/OptionsSheets.{h,cpp}` (`COptionsDrawing` page),
  `src/OptionsPropertySheet.{h,cpp}` (page registered)
- `src/Option.{h,cpp}` (wire/cable width, note defaults, component-label
  font; registry-backed)
- `src/DrawLine.cpp` (wire/cable width + cable colour),
  `src/DrawNoteText.cpp` (note defaults), `src/DrawMethod.cpp` (label
  font)
- `src/UserColor.{h,cpp}` (`CABLE` colour),
  `src/ConCad.rc` + `src/resource.h` (`IDD_OPTIONS_DRAWING`)

---

## 9. Export as PDF

A new **File → Export as PDF…** command (`ID_FILE_EXPORTPDF`) writes the
whole design to a single PDF, **one page per sheet**.

### How it works

- Renders through the existing `CContext` pipeline onto a device context
  created for the built-in **"Microsoft Print to PDF"** printer driver, so
  the output is true vector graphics (lines and text stay crisp and
  selectable) with no third-party PDF library.
- Each PDF page is set to the **same physical size and orientation as the
  sheet's page setup** (e.g. A3 landscape). The sheet's mm dimensions are
  matched to a standard paper-size code (`DMPAPER_A4`/`A3`/`A2`/`Letter`/…)
  written into the print `DEVMODE` with `dmOrientation`, applied per sheet
  via `ResetDC`, so mixed-size designs export correctly. (The "Microsoft
  Print to PDF" v4 driver ignores *custom* `dmPaperWidth/Length` at print
  time and falls back to Letter, so standard codes are used; non-standard
  pages such as A1/A0 fall back to a best-effort custom size.)
- The drawing is scaled to fit that page (preserving aspect ratio, centred);
  because the page matches the sheet, it fills the sheet with only the
  driver's small unprintable margin around it.
- The output path is passed in `DOCINFO.lpszOutput`, so the driver writes
  straight to the chosen file instead of showing its own Save dialog.

### Requirements / fallback

- The "Microsoft Print to PDF" driver ships with **Windows 10 and later**
  (an optional feature, normally on). If it is missing, the command shows a
  message explaining how to enable it and does nothing else.

### Where it lives in the UI

- **File → Export as PDF…**
- A **PDF button on the main toolbar**, immediately to the right of Save.

### Files changed

- `src/ConCadView.{h,cpp}` (`OnFileExportpdf`, `SetDevModePageSize`,
  menu/toolbar/message-map wiring, `#include <winspool.h>`)
- `src/ConCadDoc.{h,cpp}` (`SavePDFPage` — fit-to-page render of one sheet)
- `src/ConCad.rc` + `src/resource.h` (`ID_FILE_EXPORTPDF`, menu item,
  `IDR_MAINFRAME` toolbar button + tooltip/status string)
- `src/res/Toolbar.bmp` (extended 112→128 px: a PDF tile inserted at
  index 3, after Save)

---

## 10. File → Create version / Edit file (write-protected versions)

Released versions of a design are frozen as separate, write-protected files.

- **File → Create Version…** asks for a version (pre-filled with the current
  Revision; the dialog shows the resulting file name). It sets **Revision**
  to the version and **Date** to today (`YYYY-MM-DD`) on every sheet — this
  is the only place Date is set; it is read-only in Design Details — and
  saves the design as `Name_<version>.con` next to the current file. That
  file becomes the open document, write-protected: the title bar shows
  `[Write protected]`. An existing version file is never overwritten.
- The same dialog requires **Revised by** (remembered from last time; the
  Windows user name the first time) and a **Change description** (may span
  several lines). Each version adds a record — Rev, Issue date, Change
  description, Revised by — to the design's **revision history**, stored in
  every sheet's `<DETAILS>` and never edited afterwards.
- **File → Edit File** (only on a write-protected version) saves a copy as
  `Name_<version>_working.con`, clears the protection, and continues with
  that copy. If the working copy already exists you can open it or replace
  it with a fresh copy.
- Creating the next version from `Name_1.0_working.con` saves
  `Name_1.1.con` (the `_<version>_working` suffix is dropped from the base
  name, whatever Revision says). From a file not named `…_working`, the
  whole file name is the base. A version cannot contain `_`, so the
  suffix is unambiguous.

On a write-protected version only viewing and output work: zoom/pan, Find,
Select All + Copy, print, export (image/PDF), netlist/BOM/SPICE/VHDL, Save
As, Close, and Edit File. Every other command of the drawing view and the
design (tools, edit, undo, delete, paste, sheets, Design Details, Page Setup,
Import, Save, …) is greyed out, and clicks on the drawing do nothing (a
double-click explains how to get a working copy). The version file is never
saved over; in-memory side-effects such as ERC markers are discarded on close.

### Drawing-Details title block

`templates/title-blocks/Drawing-Details.svg` (bundled, 69 × 51 mm, the
user's design) is a Drawing Details panel filled automatically:

| Panel field | Token |
|---|---|
| Drawing number / title | `{DocNo} — {Title}` |
| Description | `{Description}` — new field on the Design Details → Design tab |
| Company | `{Organisation}` |
| Original author | `{Author}` |
| Revised by | `{RevisedBy}` — reviser of the latest version |
| Current revision / Revision issue date / Sheet | `{Revision}` / `{Date}` / `{Sheets}` |

Revision-history tokens for table row *N*: `{RevN}`, `{RevNDate}`,
`{RevNDesc}`, `{RevNBy}`. A fixed table shows as many rows as the highest *N*
the SVG uses (the newest that many versions, oldest at the top; empty rows
stay blank); a growing table is described below. Multi-line change
descriptions render as multiple lines (SVG text containing a line break is
now drawn line by line). These names are built-ins, so they never appear as
user variables.

### Revision history table (Edit → Revision History)

A revision-history table drawn on the **first sheet**, separate from the
title block:

- **Edit → Revision History** shows or hides it (checked = shown); the
  first time, it is placed at the bottom-left of the first sheet. Select and
  drag it like any other object. Showing/hiding switches to sheet 1 and can
  be undone; it is greyed out on write-protected versions.
- It is drawn from the **linked template `templates/revision.svg`** (layout
  from the user's *Revision history.svg*): column labels once at the top,
  then one value row per version — newest five, oldest at the top. It
  grows upwards from where it is placed. First match wins:
  `%APPDATA%\ConCAD\templates\revision.svg` (per-user),
  `<exe-dir>\templates\revision.svg` (installed),
  `<exe-dir>\..\templates\revision.svg` (dev build). Edit the file and
  open designs pick up the change (it is re-read when its timestamp
  changes). Each design also saves a copy of the template, used on machines
  without the file.

The growth comes from markers in the SVG, so the template can be redesigned
freely. Mark one row element (normally a `<g>` holding the row's lines and
its `{Rev1}` `{Rev1Date}` `{Rev1Desc}` `{Rev1By}` texts) and the elements
that must grow with it:

```xml
<g data-repeat="revisions" data-row-height="4.6" data-max-rows="5">
  <line data-stretch="row" … />                      <!-- column divider -->
  <text data-wrap-width="47.2" …>{Rev1Desc}</text>
  …
</g>
<rect data-stretch="rows" … />
```

**Bold / italic:** SVG text honours `font-weight` (`bold`, `bolder`,
`normal`, `lighter`, or 100–900) and `font-style` (`italic`, `oblique`),
whether given as an attribute, in a `<style>` class rule or inline
`style="…"`; word-wrap measurement uses the same font. (Previously all
template text rendered in normal weight.)

**Word wrap:** a `<text>` with `data-wrap-width="<w>"` (its local units)
is word-wrapped to that width; explicit line breaks always break. In the
revision table the change description wraps at its column (47.2 mm), and
its row grows by one line height (1.2 em) per extra line; the row's lines
marked `data-stretch="row"` (`<line>` `y2`, or a `height`) lengthen with
it, and everything below moves down.

The row is repeated once per history entry (at least once, at most
`data-max-rows`, default 5). Each copy is moved down `data-row-height`
(root viewBox units; the row's ancestors may only translate) and renumbered
to `Rev2`, `Rev3`, …. Elements marked `data-stretch="rows"` and the root
`height`/`viewBox` grow by the added rows. Inkscape keeps these `data-*`
attributes when you edit the file. (`CTitleBlockTemplateStore::
ExpandRevisionRows`; it works in title-block templates too.)

### On-disk format (additive)

```xml
<TinyCADSheets write_protected="1">
  <TinyCAD>  <!-- first sheet -->
    <REVISION_TABLE pos="10,1040" visible="1">PHN2Zy…</REVISION_TABLE>  <!-- base64 template copy -->
  ...
  <DETAILS>
    <DESCRIPTION>New Volvo buses for Bergkvara</DESCRIPTION>
    <REVISION_HISTORY>
      <ENTRY rev="R7" date="2026-06-30" by="Per Bertilsson">Updated connectors</ENTRY>
    </REVISION_HISTORY>
```

Each element is emitted only when in use (protected version / non-empty
description / at least one history entry / table placed). `REVISION_TABLE`
is a new drawing object (`ObjType` `xRevisionHistory = 144`); older builds
skip the unknown tag. `write_protected` is emitted only on write-protected versions. Older ConCAD/TinyCAD builds ignore
the attribute and open the file as editable. The protection guards against
accidental edits; it does not secure the file.

### Files changed

- `src/ConCadMultiDoc.{h,cpp}` (`m_bWriteProtected`, Create version / Edit
  file handlers, `CDlgCreateVersion`, `SetPathName` title suffix,
  `SaveModified`, `write_protected` load/save)
- `src/MultiSheetDoc.h` (`IsWriteProtected`)
- `src/Details.{h,cpp}` (`m_sDescription`, `m_oRevisionHistory`, history /
  `Description` / `RevisedBy` tokens, `IsBuiltInToken`, row count in
  `ResolveTitleBlock`), `src/DetailsPropertyPages.*` (Description field;
  reserved names = `CDetails::IsBuiltInToken`)
- `src/SvgTitleBlock.{h,cpp}` (multi-line text, `ExpandRevisionRows`),
  `src/ConCadRegistry.*` (`LastRevisedBy`),
  `templates/title-blocks/Drawing-Details.svg`, `templates/revision.svg`,
  `installer/ConCAD.nsi`
- `src/DrawRevisionHistory.cpp` (new), `src/Object.h`
  (`CDrawRevisionHistory`), `src/DrawingObject.h` (`xRevisionHistory`),
  `src/Io.cpp` (factory), `src/ConCadView.{h,cpp}` (Edit → Revision
  History), `src/ConCad.vcxproj{,.filters}`
- `src/ConCadView.{h,cpp}` (`OnCmdMsg` command gate, mouse/key gate)
- `src/ConCad.rc`, `src/resource.h` (menu items, status strings,
  `IDD_CREATE_VERSION`)

---

## 11. Module library and groups (Object → Create Module)

A **module** is a reusable piece of schematic — wires, placed symbols,
labels, text, … — stored in a library and inserted into a design like a
paste.

- **Set the library:** Options → Settings → Drawing → *Module library*
  lists the attached SQLite libraries (`.TCLib`). Choose one; modules are
  stored there.
- **Create:** select the objects, then **Object → Create Module…** (also on
  the right-click menu of a selection). The
  usual store dialog opens (titled *Store Module*, without the Connector
  box): give the module a name, description and fields, then *Store*. The
  definitions of the symbols the module uses are stored with it, so it can
  be placed in any design. No title-block details or sheet options are
  stored, so placing a module never changes the target sheet.
- **Place:** a module appears in the library panel like a symbol (with a
  preview). Double-click it: its objects follow the mouse; click to drop
  them (right-click cancels), exactly like Edit → Paste. Symbol references
  are kept as stored (re-annotate if needed).
- **A placed module is a group.** Any selection can also be made a group
  with **Object → Create Group** (Ctrl+G, or right-click a selected
  object). Create Group only groups objects on the sheet; Create Module
  stores the selection in the module library. Clicking any object of a group (or touching one with a
  selection box) selects the whole group, shown with a dotted frame; it
  moves, rotates, copies and deletes as one block. Its objects are still
  ordinary sheet objects, so wires, junctions and the netlist treat them
  as usual. The **Object** menu and the right-click menu offer:
  - **Edit Group** (or double-click the group) opens it (dashed orange
    frame): its objects can now be selected and changed one by one, and
    anything drawn, placed or pasted meanwhile joins the group. It closes
    again with **Esc** (in the select tool), a click on an object outside
    it, a double-click outside it, or **Finish Editing Group**.
  - **Ungroup** (Ctrl+Shift+G) dissolves the group for good (undoable);
    the objects become ordinary objects.
  - Creating a group from a selection that contains groups merges them.
  Copy/paste or duplicate of a group gives a new, separate group.
  Ctrl+G used to toggle the grid size; that is now only on its toolbar
  button.
- **Library window:** **Edit** (or double-click) on a module opens it in
  a design window titled *Module: name* — one sheet, no title block. Change
  it like any design (draw, place symbols, add `{Reference}`-style texts).
  **File → Save** (Ctrl+S) opens the Store Module dialog (name, reference,
  description, fields) and stores it back into the library; closing with
  changes asks first. Sheets, hierarchical symbols and Create Version are
  disabled there. *Duplicate*, *Send to library* and *Properties* copy
  the module data unchanged.
- **Library XML export / import** include modules: **Export** writes a
  `<MODULE>` element per module (the same `PPP`/`ORIENTATION`/`DETAILS`
  as a `<SYMBOL>`, then a `<TinyCAD>` document with the objects and the
  symbols, fonts and styles they use); **Import** stores them as modules
  again (SQLite libraries only — others report them as skipped). *Export
  symbol* on a single module works too. Both report how many symbols and
  modules were exported/imported, and a file that cannot be created is
  reported. Older builds ignore `<MODULE>` on import.
  (`ConCadMultiModuleDoc.{h,cpp}` is the module window and also reads and
  writes the module data for the export/import.)
- Replace Symbol refuses a module.

**Storage:** a module is a normal library record whose `[Type]` column is
`1` (symbols are `0`; the column already existed and was always 0). Its
`[Symbol].[Data]` is a `<TinyCAD>` XML document with the selected objects
plus the FONT/STYLE/FILL/IMAGE/SYMBOLDEF resources they use. Older
ConCAD/TinyCAD builds only read `[Type]=0`, so they ignore modules.

**Grouping on the sheet:** each object carries a module group id
(`CDrawingObject::m_group`, 0 = none). It is saved as an empty
`<GROUP id="n"/>` element written just before each grouped object;
older builds skip the unknown tag and load the objects as ordinary ones.
The id only has to be unique within the sheet (pasted modules get fresh
ids).

Also fixed: `CLibrarySQLite::GetMethodArchive` now reports database errors
instead of letting the exception escape.

### Files changed

- `src/Symbol.{h,cpp}` (`CSymbolRecord::is_module`), `src/LibraryStore.h`
  (`StoreModule`), `src/LibrarySQLite.{h,cpp}` (`StoreData` shared by
  symbols and modules, `[Type]` read/write, `GetMethodArchive` try/catch)
- `src/Io.cpp`, `src/ConCadDoc.h` (`SaveModuleXML`)
- `src/ConCadView.{h,cpp}`, `src/Menu.cpp` (Create Module, `PlaceModule`)
- `src/DlgUpdateBox.{h,cpp}` (module mode), `src/LibraryDoc.cpp`,
  `src/LibraryStore.cpp`, `src/DrawMethod.cpp` (module guards)
- `src/OptionsSheets.{h,cpp}`, `src/ConCadRegistry.*` (`ModuleLibrary`),
  `src/ConCad.rc`, `src/resource.h`
- Grouping: `src/DrawingObject.{h,cpp}` (`m_group`), `src/ConCadDoc.{h,cpp}`
  (group selection, open/close, ungroup, frames, `Import(stream, group)`),
  `src/Io.cpp` (`<GROUP>`), `src/Item.cpp` (click/box selection, context
  menu, double-click), `src/Paint.cpp`, `src/ConCadView.{h,cpp}`
  (Create Group/Edit/Ungroup/Finish, Esc), `src/DrawingObject.cpp`
  (`operator==` compares the group, so Undo of a group change works),
  `src/ConCad.rc` (Object menu, Ctrl+G / Ctrl+Shift+G)
- `src/Object.h` / `src/DrawHierarchicalSymbol.cpp`: the hand-written
  `operator=` of `CDrawLine` and `CDrawHierarchicalSymbol` now copy
  `m_group` (without it wires dropped out of their group on Undo, on a
  move that split a wire, etc.)
- Double click: the view's window class has no `CS_DBLCLKS`, so
  `CDrawEditItem::LButtonDown` detects double clicks itself (system
  double-click time and distance) — the drawing tools are unaffected.
- New default shortcuts reaching existing users: see §13.

### Module parameters (Tool Options)

A placed module has parameters like a component: **Name**, **Reference**
and the fields given in the Store Module dialog (e.g. *Package*). Clicking
the module shows them in **Tool Options** (*Module Tool Options*): click a
value to change it, **Add** / **Delete** the module's own fields (Name and
Reference stay). A text or note **inside the module** shows a value by
its name in braces — `{Reference}`, `{Name}`, `{Package}` — and updates as
soon as the value changes. Names are case-insensitive; a `{token}` that is
not a module parameter is still a design variable (§1). Module
parameters do not appear as design variables in Design Details.

- Placing a module from the library fills the parameters from the
  library record (its name, reference and fields).
- Create Module on a placed module pre-fills the Store Module dialog with
  its current parameters; the texts keep their `{tokens}`.
- Undo: all changes made in the panel while the module stays selected are
  one undo step.
- **Ungroup** or **Create Group** on a module writes the current values
  into its texts (the `{tokens}` are replaced) and drops the parameters.
- Copy/paste and duplicate copy the parameters with the module.

**Storage:** a `<MODULEINFO>` element in the module's group (after its
`<GROUP id>` tag) with one `<FIELD name="..." value="..."/>` per
parameter. It is a new drawing object, `CDrawModuleInfo` (`ObjType`
`xModuleInfo = 145`): never drawn, not clickable, a "construction" object
(so it is not printed and not stored in library modules). Older builds
skip the tag: the texts then show the raw `{tokens}`.

Files: `src/DrawModuleInfo.{h,cpp}`, `src/EditDlgModuleEdit.{h,cpp}`
(new), `src/EditToolBar.{h,cpp}`, `src/Item.cpp`
(`UpdateModulePanel`), `src/ConCadDoc.{h,cpp}` (`GetModuleInfo`,
`ResolveText`, baking on Ungroup), `src/DrawText.cpp`,
`src/DrawNoteText.cpp`, `src/DetailsPropertyPages.cpp`, `src/Io.cpp`,
`src/ConCadView.cpp`, `src/ConCad.rc` (`IDD_MODULE_EDIT`).

---

## 12. Object colours (Object → Colour)

Components, wires, cables, buses, lines, polygons and rectangles/ellipses
can be given their own colour: select them (one or many), then
**Object → Colour** or right-click → **Colour**:

- **Consat** (blue) and **Factory** (red). The two shades are set under
  Options → Settings → Drawing → *Object colours*; the default is pure
  blue / red. The chosen RGB value is stored in the object, so changing
  the setting later affects only objects coloured afterwards.
- **Custom…** — any colour (colour dialog).
- **Default Colour** — back to the normal colours.

Undoable. Polygons and rectangles change their outline only; fills keep
their colour. Components are tinted completely (as the old connector
colour did). Any component can now be coloured — also from its
properties panel; the library *Connector* flag is no longer needed for
this (it is still stored). Hierarchical-design symbols cannot be
coloured.

**Storage:** the `use_color="1" color="<COLORREF>"` attributes that
`<SYMBOL>` already had (§5) are now also written on `<WIRE>`, `<CABLE>`,
`<BUS>`, `<LINE>`, `<POLYGON>`, `<RECTANGLE>`/`<ELLIPSE>` — only when set.
Older builds ignore them and show the normal colour.

**Code:** `CDrawingObject::m_use_color` / `m_color` (replaces
`CDrawMethod::m_use_connector_color` / `m_connector_color`),
`CanColor()`, `SaveColorXML` / `LoadColorXML`; `CForcedColorScope` and a
fills-too flag on `CContext::SetForcedColor`; `CConCadDoc::SetSelectionColor`;
Consat/Factory shades in `CConCadRegistry` (`ConsatColor`, `FactoryColor`).

---

## 13. Keyboard shortcuts (Options → Keyboard Shortcuts…)

- **Ctrl+F is Flip** now. Find has no shortcut (Edit → Find…).
- **Options → Keyboard Shortcuts…** lists every command — the menus
  (in menu order), the toolbar buttons (drawing tools etc.) and anything
  else that has a shortcut — with its shortcuts. Select a command, click
  in *New shortcut*, press the keys, **Assign**. A key already in use asks
  before it is moved. **Remove** deletes the command's shortcuts, **Reset
  All** goes back to the defaults in `ConCad.rc`. OK applies and saves.
- The menus (including right-click menus) show the current shortcuts,
  not the text written in the menu resources (`CMainFrame::OnInitMenuPopup`).

**Storage:** MFC's keyboard manager keeps the table under
`HKCU\Software\ConCAD\ConCAD\Workspace\Keyboard-0` and restores it at
start-up — so a newer build's new default shortcuts would never appear.
`ApplyNewDefaultShortcuts` (`src/ConCad.cpp`) therefore applies each new
default once (profile value `Keyboard\Version`), replacing whatever used
that key and keeping the user's other shortcuts. **When you change the
accelerator table in `ConCad.rc`, add the change to that list and raise
`latest`.** (The previous build deleted the saved table once instead.)

Files: `src/ShortcutsDlg.{h,cpp}` (new), `src/MainFrm.{h,cpp}`,
`src/ConCad.cpp`, `src/ConCad.rc`, `src/resource.h`.

---

## 14. SPICE prologue/epilogue priorities removed

The `$$SPICE_PROLOG_PRIORITY` / `$$SPICE_EPILOG_PRIORITY` fields are gone:
the Store Symbol dialog no longer has the two priority boxes and no longer
writes these fields into every symbol. The SPICE netlist writes
prologues and epilogues in drawing order, each once; those of symbols
without a SPICE model (the RUN node) come first / last, which is what the
priority 0 was used for. Empty prologues no longer add blank lines.
Existing symbols keep the old fields until they are stored again (the
dialog drops them); they are ignored. Files: `src/Net.h`,
`src/NetList.cpp`, `src/DlgUpdateBox.{h,cpp}`, `src/ConCad.rc`,
`src/resource.h`, `manual/ConCAD.html`.

---

## 15. Autosave and crash recovery

The TinyCAD autosave (Options → Settings → Backup, every N minutes,
default 10) is extended:

- It writes only designs with **unsaved changes**, and never a
  write-protected version.
- A saved design backs up to `<name>.con.autosave` next to it, as before.
  An **untitled** design now backs up too, to
  `%APPDATA%\ConCAD\Recovery\Untitled-<pid>-<n>.con`.
- The backup is **deleted** after a Save (not Save a Copy As) and when the
  design is closed, so one is only left behind by a crash.
- **Opening** a design whose `.autosave` is newer than the file asks
  whether to open the autosaved changes (the design is then marked
  changed — Save to keep them) or the file as last saved (the autosave is
  deleted). An older `.autosave` is deleted silently.
- **At start-up**, recovery files of untitled designs whose ConCAD is no
  longer running are offered: Yes opens each as an untitled *Recovered N*
  design; No deletes them. The process id in the name keeps a second
  running ConCAD from offering another one's live files.

Files: `src/ConCadMultiDoc.{h,cpp}`, `src/ConCad.cpp`.

---

## File-format compatibility

All changes are **additive** to the XML `.dsn` format. Files saved by
the modified build remain compatible with prior versions in the
following sense:

- New elements (`<USERTOKEN>`, `<CABLE>`, `<CONNECTOR>`,
  `<TITLEBLOCK_SVG>`, `<GROUP>`, `<MODULEINFO>`, the new `use_color`/`color` attributes on
  `<SYMBOL>`, wires/cables/lines, `<POLYGON>` and rectangles/ellipses,
  `write_protected` on `<TinyCADSheets>`) are emitted only when
  the corresponding feature is in use.
- A pre-existing file with no new elements loads unchanged.
- Saving a file with the new build, then loading it in the new build
  again, round-trips cleanly.

The legacy binary format (`CStream` `Read`/`ReadEx` paths) was not
modified. Tokens, cables, and connector color overrides only exist in
XML-saved files.

---

## Quick test plan after rebuild

1. **Tokens** — set Title to `Hello {Foo}` (Design Details, Design
   tab). `Foo` appears under Variables; give it `bar`, OK. Title block
   (and any `{Foo}` in free Text / Notes) renders `Hello bar`. Save and
   reopen; the value persists. Remove every `{Foo}` reference → it drops
   off the list on next open.
2. **Sheets X of Y** — open a multi-sheet design. Each sheet's title
   block shows `<N> of <total>`. Add a sheet; numbers update.
3. **Shared fields** — set Title on sheet 1, OK; switch to sheet 2 —
   same Title. Repeat for a token.
4. **Cable** — Shift+F2, click two pin points; cable renders thicker.
   Run netlist; the two pins share a net. Save, reopen in Notepad,
   look for `<CABLE …/>`.
5. **Connector** — flag a library symbol as connector, place an
   instance, double-click → Color button enabled, pick a color. Other
   instances stay unchanged. Save, reopen — color persists.
6. **SVG title block** — Design Details → Title Block → pick `Simple-A4`
   → OK; the SVG title block renders bottom-right and resolves tokens.
   Save/reopen — still there (inspect `.con` for `<TITLEBLOCK_SVG>`).
   **File → New** — the picker appears; choose `Simple-A4` → the new
   design opens with it. Ctrl+N again — `Simple-A4` is preselected; Cancel
   opens nothing. Tick *Don't ask again* → next Ctrl+N opens directly with
   the remembered block; re-enable under Options → Settings → Drawing.
7. **Drawing defaults** — Options → Settings → Drawing: change wire/cable
   width and cable colour (live update); toggle the new-note Background
   fill / Rounded corners (affects newly placed notes); pick a Component
   label font (labels redraw, size unchanged).
8. **Export as PDF** — set page setup to e.g. A3 Landscape, then
   File → Export as PDF…, choose a path. The resulting PDF page measures
   A3 (420×297 mm) in landscape — one page per sheet, each in its own
   page size/orientation; title blocks resolve tokens and "Sheets N of M".
   Zoom in to confirm lines/text are vector (not rasterised).
9. **Create version** — open/save `Board.con` with Revision `R6`; in
   Design Details pick the `Drawing-Details` title block and fill in
   Description (shown in the panel). File → Create Version…: field shows
   `R6`, type `R7` → "Saved as" shows `Board_R7.con`; Revised by is
   pre-filled; OK with an empty Change description → refused; enter a
   description, OK. Title bar: `Board_R7.con [Write protected]`; the panel
   shows Revised by, Current revision R7 and today's date. Tools, Delete,
   Undo, Save and Design Details are greyed; zoom, print and Export as PDF
   work. Close and reopen — still protected, history intact. File → Edit
   File → `Board_R7_working.con`, editable. Create Version `R8` from it →
   `Board_R8.con`. Creating `R8` again is refused.
10. **Revision history table** — Edit → Revision History: the table
    appears bottom-left on sheet 1 (menu item checked) with one empty row.
    Drag it elsewhere; Undo moves it back. Create Version → one filled row;
    Edit File + Create Version twice more → three rows, growing upwards.
    After a sixth version it shows the newest five. A long change
    description wraps inside its column and makes only its own row taller. Edit → Revision History
    again hides it. Edit `templates/revision.svg` (e.g. the heading) and
    the open design updates. Save, reopen — position, visibility and rows
    are kept.
11. **Modules** — Options → Settings → Drawing: pick a `.TCLib` as Module
    library. Select a few wires and two placed symbols, Special → Create
    Module…: dialog titled *Store Module*; name it `Test module`, Store.
    It appears in the library panel with a preview. Open another design,
    double-click it: the objects follow the mouse, click to drop; the
    symbols draw correctly; title block, grid and colours unchanged. Undo
    removes it. Library → Libraries → Edit the module library: Edit on the
    module shows a message; Duplicate makes "Copy of Test module";
    Properties lets you rename it.
12. **Groups** — the Object menu exists (Create Group, Ungroup, Edit
    Group, Finish Editing Group, Create Module…) and Special no longer has
    Create Module. Place `Test module`. Click one of its wires: the whole
    module is selected with a dotted frame; drag it — everything moves,
    wires connected from outside stretch. Box-select touching one symbol
    of it selects all of it. Ctrl+R rotates it as one; Delete removes all
    of it; Undo brings it back. Ctrl+C / Ctrl+V gives a second, separate
    group. Double-click it: orange dashed frame; click single parts and
    move/edit them; draw a wire inside — it joins the group. Esc → frame
    gone, it is a block again (including the new wire). Double-click it
    again, then double-click empty space → closed. Right-click → Edit
    Group, then right-click → Finish Editing Group also works. Select a
    few loose wires and a symbol, right-click one of them: Create Group
    and Create Module… are offered; Ctrl+G groups them. Ctrl+Shift+G
    ungroups → parts select individually; **Undo regroups all of them,
    wires included**, and only that step is undone. Save, close, reopen —
    still grouped. Open the saved file in an older ConCAD build: it loads,
    ungrouped.
13. **Component colour** — place any symbol (not marked as connector),
    open its properties: the colour button is enabled; set a colour. Save,
    reopen: colour kept.
14. **Object colours** — Options → Settings → Drawing: *Object colours*
    group with Consat/Factory buttons. Select a symbol, a wire, a cable,
    a filled rectangle and a polygon; right-click one → Colour → Consat:
    all blue, the rectangle's fill unchanged. Object → Colour → Factory:
    red. Custom… → pick green. Undo steps back one colour at a time.
    Default Colour → normal again. Selection highlight still shows while
    selected. Save, reopen — colours kept. Print / Export PDF show them.
15. **Shortcuts** — Ctrl+F flips the selection; Edit → Find… has no
    shortcut shown. Options → Keyboard Shortcuts…: the list shows menu
    commands and toolbar tools; give *Wire* (toolbar) Ctrl+W → Assign;
    assign Ctrl+F to something else → asked whether to move it. OK: the
    menus show the new keys, the keys work. Restart ConCAD: still there.
    Reset All → defaults back.
16. **Module parameters** — draw a small circuit with a text
    `Ref: {Reference}  Pkg: {Package}`; select it, Create Module…: Name
    `Test2`, Reference `M?`, add a field *Package* = `TO-220`, Store.
    Place `Test2`, click it: Tool Options shows *Module Tool Options* with
    Name, Reference, Package; the text shows `Ref: M?  Pkg: TO-220`.
    Change Reference to `M1` → the text updates at once. Add a field
    `Voltage`, type `{Voltage}` into a new text inside the module (Edit
    Group) → shows the value. Undo reverts the panel changes. Copy/paste
    the module: the copy has its own values. Design Details does not list
    Reference/Package as variables. Save, reopen — values kept. Ungroup →
    the text now literally reads `Ref: M1 ...`.
17. **Edit a library module** — Library → Libraries → Edit the module
    library; double-click `Test2`: window *Module: Test2* with its objects,
    no title block. Move a symbol, add a wire, Ctrl+S → Store Module
    dialog → Store. Place `Test2` in a design: the change is there. Close
    the module window after another change → asked to save. The sheet-tab
    menu cannot add sheets there.
18. **Library XML** — in the module library window: File → Export
    library: message "Exported 0 symbol(s) and N module(s)"; the .xml has
    `<MODULE>` elements. Create a new empty .TCLib, attach it, open it,
    Import that .xml: "Imported 0 symbol(s) and N module(s)"; place one
    from there — identical. Export a normal symbol library: symbols count
    matches and re-imports as before.
19. **Autosave** — Options → Settings → Backup: every 1 minute. Open a
    saved design, move something, wait a minute: `<name>.con.autosave`
    appears; Ctrl+S: it disappears. Change again, wait, then end ConCAD
    in Task Manager. Reopen the design: asked about the autosaved changes;
    Yes shows them with the design marked changed. Leave a design open
    unchanged for a minute: no autosave file.
20. **Untitled recovery** — File → New, draw something, wait a minute:
    a file appears in `%APPDATA%\ConCAD\Recovery`. Kill ConCAD, start it:
    asked about 1 unsaved design; Yes opens *Recovered 1* with the
    drawing; Save asks for a name and the recovery file is gone. Close a
    changed untitled design with Don't Save: its recovery file is deleted.
