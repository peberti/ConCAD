# TODO

Tracked work items for ConCAD. See `HANDOFF.md` and `CHANGES.md` for background.

## Installer: complete the NSIS uninstall section — done

`Section Uninstall` in `installer/ConCAD.nsi` now reverses the install:
installed files, `$INSTDIR`, Start Menu shortcuts and folder, the uninstall /
app-path registry keys, and the `.con` → `ConCAD Design` association (`.con` is
only cleared if it still points at ConCAD). Needs a manual install → uninstall
check with NSIS once the build machine is set up again (see `SETUP.md`).

## File → Create version / Edit file / write-protection — done

Implemented; see `CHANGES.md` §10. Not done from the wish list: clicking the
`[Write protected]` text in the title bar to start editing (use
`File → Edit File`).

## Module Library — done

Implemented; see `CHANGES.md` §11 (Object → Create Module, module library
chosen in Options → Settings → Drawing, placed like a paste).

## Module behaviour — done 2026-10-08 (untested in the app)

When inserting a module, keep it as a block grouped togheter. right click allows you to edit it. 

Implemented: a placed module is a group (selected/moved/deleted as one);
Object menu (Create Group Ctrl+G, Ungroup Ctrl+Shift+G, Edit Group,
Finish Editing Group, Create Module), same on the right-click menu;
double-click opens a group, Esc / double-click outside closes it. Fixed:
Undo after Ungroup lost the wires from the group. See `CHANGES.md` §11
and quick test plan step 12.


## Connector flag does not reach placed symbols (found 2026-10-07) — fixed 2026-10-08

`GetDesignSymbol` (`Symbol.cpp`) now copies `is_connector` into the
`CDesignFileSymbol`, and `CDesignFileSymbol::SaveXML/LoadXML` write/read a
`<CONNECTOR>1</CONNECTOR>` element (only when set). Compiles; needs an app
check: mark a library symbol as connector, place it, open its properties —
the colour button should be enabled; save, reopen, still enabled. Symbols
placed before the fix carry no flag in the design and must be re-placed.


## ctrl+f flip — done 2026-10-09 (Find has no shortcut now; CHANGES.md §13)

## copy and cut text objects — done 2026-10-21 (untested in the app; CHANGES.md §16)

When pressing ctrl+c or ctrl+x on a text box, label, etc. the object shall be processed, not the text itself. 

## dynamic shortcuts — done 2026-10-09 (Options → Keyboard Shortcuts…; CHANGES.md §13)

From the Options menu assign key shortcuts to commands

## Change colors — done 2026-10-09 (Object → Colour / right-click; CHANGES.md §12)

Rightclick any component, wire, cable, polygon and rectangle and choose "Consat" to have it blue, "Factory" to get i red, custom -> choose any color.   Also When multiple Object is selected

Consat/Factory shades are set in Options → Settings → Drawing; outline
only for filled shapes; works on every component (not only connectors).


## Module parameters in Tool Options — done 2026-10-09 (untested in the app)

Click a module brings up the same dialog as clicking components "Tool
Options", with parameters like Reference, Name, Package, so a text in the
module can be updated from the Tool Options dialog.

Implemented: Module Tool Options panel (Name, Reference, module fields;
Add/Delete); texts/notes in the module show `{Field}`. See `CHANGES.md` §11
"Module parameters" and quick test plan step 16.

## Edit modules like components + XML export — done 2026-10-09 (untested in the app)

Library window → Edit/double-click opens a module in a "Module: name" window;
Save stores it back. Library XML export/import includes modules. See
`CHANGES.md` §11 and quick test plan steps 17–18.

## New function, align to grid. 

Implement an **Align to Grid** command in TinyCAD based on the following requirements.

**Scope and controls**

- Align selected objects, or the entire drawing if nothing is selected.
- Allow applying it to the active sheet or all sheets.
- Use the current grid spacing, with an option to enter another spacing.
- Use TinyCAD’s native grid-unit conversion. In the metric spacing box, a value of 10 means 1 mm.

**Alignment behavior**

- Align components by their electrical pin connection points, not their bounding boxes or insertion points.
- Move each component as a whole, preserving its symbol geometry, pin spacing, rotation, mirroring, and scaling.
- If all pins cannot fit the chosen grid through translation alone, leave the component unchanged and highlight it. Explain whether the issue is incompatible pin spacing or pins closer together than the grid spacing.
- Snap wire endpoints and bends to the grid. Move connected endpoints together with their component pins.
- Preserve horizontal and vertical routing where possible. Adjust nearby bends or add short orthogonal segments when needed.
- Keep junctions, electrical labels, and no-connect markers attached to their original connections.
- Remove zero-length wire segments created by alignment only when doing so preserves connectivity.
- Preserve text content, fonts, colors, line styles, and other formatting. Font cleanup is a separate function.

**Connectivity protection**

- Record electrical connectivity before making changes.
- Preserve existing connections and keep separate nets separate. Nearest-point rounding alone is insufficient: it can accidentally join neighboring wires.
- When snapping would create an unintended contact, try a nearby grid point or reroute locally. If no safe solution exists, leave the affected objects unchanged and highlight them.
- Detect small pre-existing wire-to-pin gaps that alignment could repair. Show these as proposed connection changes and require explicit acceptance before adding those connections.
- Use native symbol transformations and connectivity logic, including rotated, mirrored, scaled, and multipart symbols.

**User experience**

- Preview moved objects, unresolved objects, and proposed connection repairs before applying.
- Apply the operation as one undoable transaction.
- Show a brief result summary: components moved, wires adjusted, redundant segments removed, connection repairs accepted, and objects left unresolved.
- Do not overwrite or save the drawing automatically.

**Validation**

Add tests covering off-grid components, rotated and mirrored symbols, scaled symbols, incompatible pin pitches, junctions, wire crossings, labels, no-connect markers, short segments, and closely spaced separate nets. Verify that accepted changes land on the grid and that connectivity changes only where the user explicitly accepted a repair.

## generate BoM

a tool to extract all part numbers from all text object (text boxes, variables, labels, etc.) based on prefix like "CON-". The prefix shall be configurable in the settings menu.  The output shall be an excel file. 
if tokens exists for project details such as: "Project name", "Customer", "Project number", etc. and Vehicle details such as: "Vehicle manufacturer", "vehicle model", "vehicle length", "quantity", etc. This should be included in the top of the BoM excel file. Default storage location is same as the .con file. Default filename should be configurable with all avilible tokens. Se sample file "BOM_Bergkvara_Trelleborg_Volvo_R7" 

As a BOM template, an existing excel file with formatting, text, formulas and tokens shall be placed somewhere in the main or subdirectory of the applikation executable. 

Shall be availible with a button from the toolbar. 

## Design details toolbar button — done 2026-10-21 (untested in the app; CHANGES.md §16)

Shall be availible with a button from the toolbar.