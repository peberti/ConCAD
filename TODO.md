# TODO

Tracked work items for ConCAD. See `HANDOFF.md` and `CHANGES.md` for background.

## Installer: complete the NSIS uninstall section — done

`Section Uninstall` in `installer/ConCAD.nsi` now reverses the install:
installed files, `$INSTDIR`, Start Menu shortcuts and folder, the uninstall /
app-path registry keys, and the `.con` → `ConCAD Design` association (`.con` is
only cleared if it still points at ConCAD). Needs a manual install → uninstall
check with NSIS once the build machine is set up again (see `SETUP.md`).

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