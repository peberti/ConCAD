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

You can now define your own named text variables (tokens) in the design
details dialog and reference them with `{TokenName}` syntax inside any
title-block text field. They get substituted at render time.

### Where it lives in the UI

`File → Design Details…` (Ctrl+D) now has three tabs:

- **Design** — the existing title-block fields (Title, Author,
  Revision, Document, Organisation, Date) and the visibility checkbox.
- **Guides** — the existing rulers configuration.
- **Variables** — *new*. A list of user-defined name/value tokens with
  Add / Edit / Remove buttons.

### How to use

1. Open `File → Design Details…` and switch to the **Variables** tab.
2. Click **Add…** to define a token. Give it a name (letters, digits,
   and underscores; cannot start with a digit) and a value.
3. Reference the token inside any title-block field as `{TokenName}`.
   Example: set Title to `Demo for {ProjectCode}` and define
   `ProjectCode = PRJ-001`. The title block will render
   `Demo for PRJ-001`.

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
  which catches cycles safely.
- Unknown tokens are left as-is (literal `{XYZ}`).
- Tokens are stored inside each sheet's `<DETAILS>` block as
  `<USERTOKEN name="...">value</USERTOKEN>` entries. Old `.dsn` files
  without tokens still load.

### Files changed

- `src/Details.h`, `src/Details.cpp`
- `src/DetailsPropertyPages.h`, `src/DetailsPropertyPages.cpp`
- `src/DetailsPropertySheet.h`, `src/DetailsPropertySheet.cpp`
- `src/TinyCad.rc` (new dialog templates `IDD_DETAILS_PAGE3`,
  `IDD_EDIT_TOKEN`)
- `src/resource.h` (new resource IDs)

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

### What is **not** shared (still per-sheet)

- Page size
- Ruler / guides configuration
- Title-block visibility checkbox
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

## File-format compatibility

All changes are **additive** to the XML `.dsn` format. Files saved by
the modified build remain compatible with prior versions in the
following sense:

- New elements (`<USERTOKEN>`, `<CABLE>`, `<CONNECTOR>`, the new
  `use_color`/`color` attributes on `<SYMBOL>`) are emitted only when
  the corresponding feature is in use.
- A pre-existing file with no new elements loads unchanged.
- Saving a file with the new build, then loading it in the new build
  again, round-trips cleanly.

The legacy binary format (`CStream` `Read`/`ReadEx` paths) was not
modified. Tokens, cables, and connector color overrides only exist in
XML-saved files.

---

## Quick test plan after rebuild

1. **Tokens** — Design Details → Variables → Add `Foo = bar`. Set
   Title to `Hello {Foo}`. OK. Title block renders `Hello bar`. Save
   and reopen; tokens persist.
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
