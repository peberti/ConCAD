# ConCAD changes

This document describes the features added in this round of development,
how they work, and how to test them. All changes are in the `src/`
directory of the ConCAD project. The XML `.dsn` file format is the only
serialization format affected; the legacy binary format is unchanged.

Build with Visual Studio (Community 2019/2022 with the MFC component
installed). Open `TinyCad.sln` at the repo root, then Build → Build
Solution (Ctrl+Shift+B).

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
- `src/TinyCad.rc` (Design page hosts the Variables editor; Design
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
- `src/TinyCadDoc.cpp` (compute index/total before
  `GetDetails().Display`)
- `src/Io.cpp` (compute index/total before `WriteXML`)
- `src/DetailsPropertyPages.cpp` (read-only field, no longer written
  on Apply)
- `src/TinyCad.rc` (label change, `ES_READONLY` on the edit field)

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
`CTinyCadMultiDoc` already initializes the new sheet's `CDetails` from
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
- `src/DragUtils.cpp`, `src/JunctionUtils.cpp`, `src/TinyCadDoc.cpp`
  (drag/junction/snap paths include xCable alongside xWire)
- `src/TinyCadView.h`, `src/TinyCadView.cpp` (new `OnSelectCable`
  handler, message-map entry)
- `src/resource.h` (new `IDM_TOOLCABLE = 32908`)
- `src/TinyCad.rc` (Shift+F2 accelerator, status-bar string)

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
- `src/TinyCad.rc` (new "Co&lor…" button in `IDD_METHOD`, new
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

### Files changed

- `src/SvgTitleBlock.{h,cpp}` (NanoSVG-based renderer + template store),
  `src/nanosvg/nanosvg.h` (vendored)
- `src/Details.{h,cpp}` (`m_sTitleBlockName` / embedded copy /
  `m_sEffectiveSvg`, `ResolveTitleBlock`, `DisplayBox` SVG branch)
- `src/DetailsPropertyPages.*`, `src/TinyCad.rc` (the Title Block tab)

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
  `src/TinyCad.rc` + `src/resource.h` (`IDD_OPTIONS_DRAWING`)

---

## File-format compatibility

All changes are **additive** to the XML `.dsn` format. Files saved by
the modified build remain compatible with prior versions in the
following sense:

- New elements (`<USERTOKEN>`, `<CABLE>`, `<CONNECTOR>`,
  `<TITLEBLOCK_SVG>`, the new `use_color`/`color` attributes on
  `<SYMBOL>`) are emitted only when the corresponding feature is in use.
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
   Save/reopen — still there (inspect `.dsn` for `<TITLEBLOCK_SVG>`).
7. **Drawing defaults** — Options → Settings → Drawing: change wire/cable
   width and cable colour (live update); toggle the new-note Background
   fill / Rounded corners (affects newly placed notes); pick a Component
   label font (labels redraw, size unchanged).
