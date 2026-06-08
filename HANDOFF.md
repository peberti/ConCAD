# Session handoff — SVG title-block feature

## Last commit on master
`e3e66a6` — *Don't throw from ~CppSQLite3DB (fixes abort() on library delete)*

Commits added this session (build bring-up on a fresh machine — see "Build & runtime bring-up" below):
- `4c04216` — Fix crash and read-only failures when opening SQLite libraries
- `e3e66a6` — Don't throw from ~CppSQLite3DB (fixes abort() on library delete)

`master` is ahead of `origin/master` and unpushed — pushing is the user's call. The SVG title-block WIP (step 4/4b) is still **staged but uncommitted** in the working tree (alongside `HANDOFF.md`, `CHANGES.md`, `manual/ConCAD.html`, the `Details`/`SvgTitleBlock`/`DetailsPropertyPages` sources, and `src/TinyCad.vcxproj` which now also carries the v142 revert).

## Build & runtime bring-up (fresh VS install, this session)

- **Toolset must stay `v142`** (Win32/x86, MFC Dynamic). The vendored NuGet libs `libjpeg_static` + `libiconv.lib` (`packages/`) ship `.lib`s only for v140/v141/v142 — bumping to v143/v145 fails the link (`_jpeg_*` / `_libiconv_*` unresolved) and also needs MFC for that toolset. A stray `v145` in `src/TinyCad.vcxproj` (uncommitted working-tree drift; history was already v142) was reverted. Don't commit a VS auto-retarget.
- **VS install:** "Desktop development with C++" + **C++ MFC for the v142 toolset (x86 & x64)** — note an ATL-only install looks like MFC but isn't. Git for Windows on PATH for `gitbranch.bat`.
- **SQLite crashes fixed and committed:**
  - `CLibrarySQLite::Attach` no longer crashes when its catch-handler `close()` throws; the IsConnector migration is now best-effort (PRAGMA-probe + try/catch `ALTER`) so read-only libraries (e.g. under Program Files) load instead of failing. The `IsConnector` read is guarded because `getIntField` throws on an unknown column. (`4c04216`)
  - `~CppSQLite3DB` no longer lets `close()` throw out of the destructor (was `abort()` on library delete); now matches the sibling Query/Statement destructors. (`e3e66a6`)
- **Why the app loaded VeeCAD / TinyCAD libraries:** first-run migration in `CTinyCadApp::InitInstance` (`TinyCad.cpp:361`) recursively copies `HKCU\Software\TinyCAD\TinyCAD` → `HKCU\Software\ConCAD\ConCAD` (`SHCopyKey`), inheriting the whole library list. Prune unwanted libraries via **Library → Libraries…** (rewrites the `Libraries` value under ConCAD's own key). Deleting the ConCAD key just re-triggers the copy.
- **Latent, not fixed:** `CLibrarySQLite::GetMethodArchive` (~line 318) is the one SQLite method with no try/catch.

## Plan progress

| Step | What | Status |
|---|---|---|
| 1 | NanoSVG vendored at `src/nanosvg/nanosvg.h` + `CSvgTitleBlock` renderer (`src/SvgTitleBlock.{h,cpp}`) | done |
| 2 | `CDetails::DisplayBox` branches to SVG renderer when `m_sTitleBlockSvg` is set; rect anchored bottom-right at the SVG's natural mm dimensions | done |
| 3 | "Title Block" tab in `File → Design Details` (`CDetailsPropertyPage4`) with Browse / Use built-in | done |
| 4 | Bundled templates folder + `CTitleBlockTemplateStore` enumerator + listbox in the tab + installer hook | **done — user is testing** |
| 4b | **Hybrid storage** (name reference + base64 embedded fallback) — see below | **done — build is now green; run the §"Hybrid storage" tests** |
| 5 | `File → New` picker + registry default + "Don't ask again" | not started |
| 6 | Polish: Save-as-template, preview pane, error toasts | not started |

## Step 4b — Hybrid SVG storage (named-template-wins + base64 fallback)

**Why:** `.dsn` files live on SharePoint and are shared across machines. Pure name-reference would break when a machine lacks the template; raw-embedded SVG-in-XML was escape-fragile (the double-escape bug). Hybrid gives both portability and central updates.

**On-disk format** (additive; old files still load):
```xml
<TITLEBLOCK_SVG name="Simple-A4" enc="base64">PHN2ZyB4bWxu…</TITLEBLOCK_SVG>
```
- `name` — template stem; resolved against the store on load so a re-shipped bundled template propagates. Absent for one-off (Browse…) files.
- `enc="base64"` — child data is base64 of the SVG's UTF-8 bytes. Legacy files have no `enc` attr → read as raw, upgraded to base64 on next save.

**Code (all committed to the working tree, not yet built):**
- `CDetails`: new `m_sTitleBlockName` (serialized), transient `m_sEffectiveSvg` (not serialized); `ResolveTitleBlock()` picks named-template-from-store else embedded copy. Called at end of `ReadXML` and from the picker's `OnApply`. `DisplayBox` renders `m_sEffectiveSvg` (falls back to embedded). `CopyDesignFields`/`Reset` updated.
- `CTitleBlockTemplateStore`: `FindByName`, `EncodeSvgBase64`, `DecodeSvgBase64`; `STitleBlockTemplate.name` (stem without the ` (user)` suffix). Base64 via `CryptBinaryToStringA`/`CryptStringToBinaryA`; `#pragma comment(lib, "Crypt32.lib")` in `SvgTitleBlock.cpp` (no `.vcxproj` change).
- `CDetailsPropertyPage4`: new `m_sName`; listbox select sets it, Browse/Use-built-in clear it, `OnInitDialog` preselects the matching row, `OnApply` writes name+svg and calls `ResolveTitleBlock()`.

**Hybrid storage tests (do after a Build Solution):**
1. Pick `Simple-A4` → save → reopen: renders; inspect `.dsn` → `<TITLEBLOCK_SVG name="Simple-A4" enc="base64">`.
2. Rename/remove `Simple-A4` from the store → reopen the same `.dsn`: **still renders** via the embedded copy.
3. Drop an edited `Simple-A4` into the store → reopen: picks up the new artwork (central update).
4. Browse an arbitrary `.svg` → save/reopen: renders, no `name` attr (embed-only).
5. Open a pre-4b `.dsn` with raw `<TITLEBLOCK_SVG>`: loads fine; re-save upgrades it to base64.

## Side fixes/detours (already merged, not undone)

- Splash rebranded to **ConCAD** (`src/Startup.cpp`). Version scheme is `0.<git-commit-count>` via `src/gitbranch.bat`, with fallback search through common Git-for-Windows install paths so MSBuild's pre-build event finds `git.exe`.
- Update-check feature removed entirely (`UpdateCheck.{cpp,h}`, `DlgUpdateCheck.{cpp,h}` deleted; menu entries / message map / handlers gone).
- Real-world Inkscape SVG fixes in `SvgTitleBlock.cpp`: parent `<g transform=…>` inheritance for `<text>`, viewBox→pixel scaling, CSS `style="…"` parser for font-size/family/fill/text-anchor, recursive `<tspan>` text collection, viewBox-aware font sizing.
- XMLWriter double-escape bug fixed: `makeString` already entity-escapes — an earlier "redundant escape" addition in `internal_addChildData` (commit `a458da2`) was reverted in `6c1f3e8`.

## Step 4 test checklist for the user

1. **Rebuild Solution** (Ctrl+Alt+F7). Splash should show `Version 0.<N>` with N > 0 (was 0.0 before the `gitbranch.bat` fix in `46507e5`).
2. Open any `.dsn`. `File → Design Details… → Title Block`.
3. Listbox shows **Simple-A4** (the bundled starter at `templates/title-blocks/Simple-A4.svg`). Click → state label changes → OK → bottom-right title block renders Simple-A4.
4. Set Title / Author / DocNo etc. on the **Design** tab — `{Title}`, `{Author}`, etc. inside the SVG resolve.
5. **User folder** — drop any `.svg` into `%APPDATA%\TinyCAD\templates\title-blocks\` (create the path if missing). Reopen Design Details. The listbox should show it with a ` (user)` suffix, sorted alphabetically.
6. **Browse SVG…** still works for arbitrary paths; **Use built-in** clears the SVG and restores the procedural box.
7. **Round-trip** — save the `.dsn`, reopen — the SVG is still in effect.

## Working files (untracked, intentional)

- `tCad1.dsn` — test design at repo root with the Inkscape SVG embedded.
- `Design_details.svg`, `Design_details - Copy.svg` — user's source SVG files.
- `src/TinyCad.aps`, `src/TinyCad.vcxproj.user` — VS-generated, expected.

## What to do next session

- The build now compiles and runs (v142 toolset; SQLite crashes fixed). The SVG step 4 / 4b tests below were **not** reached this session — the time went to build bring-up and the SQLite crash fixes — so run those next.
- Confirm step 4 test results from the user.
- If green, plan and implement **step 5**: `File → New` picker. Same listbox UI from `CDetailsPropertyPage4`, hoisted into a modal dialog (suggested `IDD_PICK_TITLE_TEMPLATE`) that pops before the empty document opens. Hook into `CTinyCadMultiDoc::OnNewDocument` or the doc-template path in `src/TinyCad.cpp`. Remember last choice in `CTinyCadRegistry` (new registry key) and offer a "Don't ask again" checkbox. With 4b done, the picker must set **both** `m_sTitleBlockName` and `m_sTitleBlockSvg` on the new doc's `CDetails`, then call `ResolveTitleBlock()` — same pair the property page writes.
- If the user later asks for the **2-unit margin** drop or other layout tweaks, that's a one-line change in `CDetails::DisplayBox` in `src/Details.cpp`.
- Optional cleanups: silence NanoSVG's `C4244` warnings via `#pragma warning(push/disable/pop)` around its `#include`; add more bundled starter templates.

## Where the plan file lives

The plan-mode artefact is at `/home/per/.claude/plans/cuddly-tickling-aho.md` — most recent content describes the variable-height SVG behaviour question. Earlier history of the plan walked through the full SVG title-block feature; it's stale relative to the current state, so trust this HANDOFF.md instead.
