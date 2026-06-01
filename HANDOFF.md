# Session handoff — SVG title-block feature

## Last commit on master
`b3db1ba` — *Step 4: bundled SVG templates folder + enumerator + listbox*

`master` is ~17 commits ahead of `origin/master` (last push was `ad1741b`, the CLAUDE.md commit). Nothing in this work has been pushed since the original CLAUDE.md push — pushing is the user's call.

## Plan progress

| Step | What | Status |
|---|---|---|
| 1 | NanoSVG vendored at `src/nanosvg/nanosvg.h` + `CSvgTitleBlock` renderer (`src/SvgTitleBlock.{h,cpp}`) | done |
| 2 | `CDetails::DisplayBox` branches to SVG renderer when `m_sTitleBlockSvg` is set; rect anchored bottom-right at the SVG's natural mm dimensions | done |
| 3 | "Title Block" tab in `File → Design Details` (`CDetailsPropertyPage4`) with Browse / Use built-in | done |
| 4 | Bundled templates folder + `CTitleBlockTemplateStore` enumerator + listbox in the tab + installer hook | **done — user is testing** |
| 5 | `File → New` picker + registry default + "Don't ask again" | not started |
| 6 | Polish: Save-as-template, preview pane, error toasts | not started |

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

- Confirm step 4 test results from the user.
- If green, plan and implement **step 5**: `File → New` picker. Same listbox UI from `CDetailsPropertyPage4`, hoisted into a modal dialog (suggested `IDD_PICK_TITLE_TEMPLATE`) that pops before the empty document opens. Hook into `CTinyCadMultiDoc::OnNewDocument` or the doc-template path in `src/TinyCad.cpp`. Remember last choice in `CTinyCadRegistry` (new registry key) and offer a "Don't ask again" checkbox.
- If the user later asks for the **2-unit margin** drop or other layout tweaks, that's a one-line change in `CDetails::DisplayBox` in `src/Details.cpp`.
- Optional cleanups: silence NanoSVG's `C4244` warnings via `#pragma warning(push/disable/pop)` around its `#include`; add more bundled starter templates.

## Where the plan file lives

The plan-mode artefact is at `/home/per/.claude/plans/cuddly-tickling-aho.md` — most recent content describes the variable-height SVG behaviour question. Earlier history of the plan walked through the full SVG title-block feature; it's stale relative to the current state, so trust this HANDOFF.md instead.
