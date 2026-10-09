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
