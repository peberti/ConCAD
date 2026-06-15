# TODO

Tracked work items for ConCAD. See `HANDOFF.md` and `CHANGES.md` for background.

## Installer: complete the NSIS uninstall section

`installer/ConCAD.nsi` registers ConCAD as a standard Windows program (HKLM
uninstall key, `.con` file association, Start Menu shortcuts), but its
`Section Uninstall` only deletes a single legacy Start Menu shortcut. A normal
uninstall therefore leaves files and registry keys behind.

Make the uninstall section reverse what install does:

- [ ] Delete installed files in `$INSTDIR` (`ConCAD.exe`, libs, etc.) and the
      `$INSTDIR` directory itself.
- [ ] Remove Start Menu shortcuts and the `$SMPROGRAMS\ConCAD` folder.
- [ ] Delete the `.con` → `ConCAD Design` association keys under `HKCR`
      (`.con`, `ConCAD Design`, and its `shell`/`DefaultIcon` subkeys) — these
      are written on install with no matching cleanup, so `.con` files point at
      a removed `ConCAD.exe` after uninstall.
- [ ] Delete the uninstall registry key (`${PRODUCT_UNINST_KEY}`) and
      `${PRODUCT_DIR_REGKEY}`.
- [ ] Use `SetShellVarContext all` (already present) so per-machine paths/keys
      resolve correctly under elevation.

## File → Create version / Edit file / write-protection

Designed but **not started** (deferred by the user). From `HANDOFF.md`:

- [ ] `File → Create version` prompts for a version string (pre-filled from
      the Revision field), writes it to Revision, stamps Date = today, and
      saves as `Name_<ver>.con`.
- [ ] That saved document becomes the open, **write-protected** document:
      internal serialized flag, `[Write protected]` shown in the title bar,
      edits gated.
- [ ] `File → Edit file` copies it to `Name_<ver>_working.con` and clears
      write-protection.

Note: the Date field being read-only in the Design Details dialog was already
done as the first step of this feature.

## Module Library

- [ ] A new kind of library but with parts of schematics stored as "modules". 
      Similar as "Symbols" Librarys but with schematics instead. Same function as 
	  `File → import` 