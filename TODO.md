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

Implemented; see `CHANGES.md` §11 (Special → Create Module, module library
chosen in Options → Settings → Drawing, placed like a paste).

## Module behaviour
When inserting a module, keep it as a block grouped togheter. right click allows you to edit it. 


## Connector flag does not reach placed symbols (found 2026-10-07)

`is_connector` is not copied into `CDesignFileSymbol` in `GetDesignSymbol`
(`Symbol.cpp` ~313) and `CDesignFileSymbol::SaveXML/LoadXML` do not write or
read it, so `CDrawMethod::IsConnector()` is probably always false and the
per-instance connector colour never becomes editable. Needs a check in the
app and a fix.
