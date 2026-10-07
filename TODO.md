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

## Module Library

- [ ] A new kind of library but with parts of schematics stored as "modules". 
      Similar as "Symbols" Librarys but with schematics instead. Same function as 
	  `File → import` 