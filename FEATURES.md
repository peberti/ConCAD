# ConCAD — new features and how to test them

ConCAD is based on TinyCAD. This guide lists everything ConCAD adds or
changes compared with TinyCAD: what each feature does, how to use it, and
what to check when testing it. It describes **version 0.1** (Test
Release); Help → About shows the exact version, e.g. `ConCAD 0.1.716
[Test Release]`.

---

## 0. Before you start

- **Install:** run `ConCAD_0.1.<n>_Test_Release.exe`. The installer is not
  signed, so Windows may say *"Windows protected your PC"* — click
  **More info → Run anyway**.
- **Libraries:** the installer includes the ConCAD libraries. On the first
  start they are copied to `Documents\ConCAD\library\` and attached
  (Library → Libraries…), and `modules` is set as the module library
  (Options → Settings → Drawing → Module library). Your changes to them
  are kept when ConCAD is reinstalled.
- **Files:** designs are saved as **`.con`**. Older TinyCAD `.dsn` files
  still open. A design saved by ConCAD still opens in TinyCAD, but without
  the ConCAD-only parts (groups, colours, cables show as normal objects).
- **Reporting a problem:** use **Help → Report a Bug…** (§19): it sends
  your description together with the logs, crash information and the
  open design. Add a screenshot to the email if it helps.

---

## 1. Design Details

**What:** the title-block information of a design (Title, Author,
Organisation, Description, Revision, Document, Date, Sheets, and your own
variables).

**How to use:**
- **Edit → Design Details…**, **Ctrl+D**, or the **Design Details** button
  on the main toolbar (after Print: a page with a blue title block).
- Tabs: **Design** (fields and variables), **Guides** (rulers),
  **Title Block** (choose the title-block template, §5).
- *Sheets (auto)* and *Date* are read-only: Sheets is computed (§3) and
  Date is set by Create Version (§6).

**Test:**
- [ ] All three ways open the dialog.
- [ ] On a write-protected version (§6) the menu item and the button are
      greyed out.

---

## 2. Design variables (tokens)

**What:** write `{Name}` in a title-block field, a free Text or a Note and
it is replaced by a value in the drawing, prints and PDFs. Names use
letters, digits and `_`, and cannot start with a digit.

**How to use:**
1. Type e.g. `Customer: {Customer}` in a Note.
2. Open Design Details → Design. `Customer` is listed under the fields;
   type a value and click OK. The Note now reads `Customer: VY`.

- Built-in names (no need to define them, upper/lower case does not
  matter): `{Title}`, `{Author}`, `{Organisation}` (or `{Org}`),
  `{Description}`, `{Revision}`, `{DocNo}` (or `{Document}`), `{Date}`,
  `{Sheets}`, `{RevisedBy}`.
- A variable can contain other variables (e.g. `Project` = `{Prefix}_{Number}`).
- **An empty value is shown as written**, e.g. `{Organisation}` or
  `{Customer}`, so you can see what is not filled in yet.
- A variable is listed only while it is used somewhere; remove the last
  `{Name}` and it disappears from the list.
- Variables are not used in net labels or pin names (the netlist is not
  affected).

**Test:**
- [ ] A new `{Name}` in a Text, a Note and a title-block field appears in
      the list; the value shows everywhere it is used.
- [ ] Empty variables and empty built-in fields show as `{Name}`.
- [ ] A variable that refers to another variable resolves fully.
- [ ] A mistyped `{Name` (no closing brace) stays as typed, and a correct
      `{Other}` after it still works and is listed.
- [ ] Save, close, reopen: values kept.
- [ ] Remove all uses of a variable: it is gone from the list next time.

---

## 3. Sheets "N of M"

**What:** `{Sheets}` shows the sheet's position, e.g. `2 of 4`, always up
to date.

**Test:**
- [ ] Each sheet of a multi-sheet design shows its own number.
- [ ] Add, delete or reorder sheets: all numbers update.

---

## 4. Design details shared by all sheets

**What:** the fields and variables of Design Details are the same on every
sheet of a design. Page size and rulers stay per sheet.

**Test:**
- [ ] Change Title or a variable on sheet 1, OK, go to sheet 2: same value.
- [ ] A newly added sheet gets the same details.

---

## 5. SVG title blocks

**What:** the title block can be drawn from an SVG template (made e.g. in
Inkscape) instead of the classic built-in box. The templates use `{Name}`
variables. Bundled templates:

| Template | Contents |
|---|---|
| `Simple-A4` | Title, author, document, sheet, revision, date, organisation |
| `Drawing-Details` | Drawing number/title, description, company, author, revised by, revision, date, sheet |
| `Design-Details` | As Drawing-Details plus Customer; title row `{Project_prefix}_{Project_number} — {Title}` |

**How to use:**
- **Design Details → Title Block:** pick a template, **Browse SVG…** for
  your own file, or **Use built-in** for the classic box.
- **File → New** (Ctrl+N) first asks which title block the new design
  gets; the choice is remembered. *Don't ask again* skips the question;
  turn it back on in Options → Settings → Drawing.
- Your own templates can be put in `%APPDATA%\ConCAD\templates\title-blocks\`;
  they appear with "(user)" after the name.
- The template is saved inside the design, so the design looks the same on
  a computer that does not have the template.

**Test:**
- [ ] Each bundled template draws bottom-right and fills in its fields.
- [ ] Design-Details: the variables `Project_prefix`, `Project_number` and
      `Customer` appear on the Design tab.
- [ ] File → New: picker shown, choice remembered, Cancel opens nothing,
      *Don't ask again* works and can be turned back on.
- [ ] Bold/italic text in a template shows bold/italic.
- [ ] Save, reopen: title block kept.

---

## 6. Versions: File → Create Version / Edit File

**What:** a released version of a design is saved as its own,
write-protected file, with a revision history.

**How to use:**
1. Design Details → fill in Revision (e.g. `R6`) and Description.
2. **File → Create Version…**: enter the new version (e.g. `R7`) — the
   dialog shows the file name, e.g. `Board_R7.con`. **Revised by** and a
   **Change description** are required. OK.
3. The version file opens **write-protected**: the title bar shows
   `[Write protected]`. Revision becomes `R7` and Date today's date on all
   sheets. You can view, zoom, find, copy, print, export PDF and make
   netlists, but not change anything.
4. **File → Edit File** creates `Board_R7_working.con`, which you can edit.
   The next Create Version from it gives `Board_R8.con`.

**Test:**
- [ ] Empty Change description is refused; an existing version file is
      never overwritten (creating `R8` twice is refused).
- [ ] On the protected file: drawing tools, Delete, Undo, Save, Design
      Details are greyed; double-click in the drawing explains how to edit.
- [ ] Zoom, print, Export as PDF still work.
- [ ] Close and reopen the version: still protected, history kept.
- [ ] Edit File: working copy is editable; if it already exists you are
      asked whether to open or replace it.

---

## 7. Revision history table

**What:** a table on sheet 1 listing the versions (Rev, date, change
description, revised by), newest five, filled from Create Version.

**How to use:** **Edit → Revision History** shows/hides it (first time
bottom-left on sheet 1). Drag it where you want it. Its look comes from
`templates\revision.svg` in the installation folder (can be edited).

**Test:**
- [ ] Shown with one empty row before the first version.
- [ ] Each Create Version adds a row; the table grows upwards; after six
      versions it shows the newest five.
- [ ] A long description wraps inside its column; only that row gets taller.
- [ ] Drag and Undo work; hide/show works; save/reopen keeps position.

---

## 8. Cable tool

**What:** a thicker, wire-like line for cables/harnesses. Electrically it
is a wire: pins connected by a cable are on the same net.

**How to use:** **Shift+F2** (no toolbar button), click start and end
points; right-click or Esc to stop. Width and colour: Options → Settings →
Drawing.

**Test:**
- [ ] Cable snaps to pins, makes junctions, follows a dragged symbol.
- [ ] Netlist: pins connected by a cable share a net.
- [ ] Width/colour settings apply to cables only.

---

## 9. Object colours

**What:** give components, wires, cables, buses, lines, polygons and
rectangles/ellipses their own colour — e.g. blue for Consat equipment,
red for factory equipment.

**How to use:** select one or more objects, then **Object → Colour** or
right-click → **Colour**: **Consat** (blue), **Factory** (red),
**Custom…** (any colour), **Default Colour** (back to normal). The Consat
and Factory shades are set in Options → Settings → Drawing → *Object
colours*. A component's colour can also be set from its properties panel.

**Test:**
- [ ] Each colour choice works on a single object and on a mixed
      selection; filled shapes change outline only, fill unchanged.
- [ ] Undo steps back one colour change at a time.
- [ ] Colours are kept after save/reopen and show in print and PDF.
- [ ] Changing the Consat/Factory shade affects only objects coloured
      afterwards.

---

## 10. Drawing settings (Options → Settings → Drawing)

**What:** one page with drawing defaults:
- Wire width and colour, cable width and colour (applied immediately);
- background fill and rounded corners for **new** note boxes;
- the font of component labels (reference/value);
- Consat/Factory colours (§9), module library (§13), "ask for a title
  block on File → New" (§5).

**Test:**
- [ ] Each setting takes effect as described and is remembered after
      restarting ConCAD.

---

## 11. Export as PDF

**What:** **File → Export as PDF…** (or the PDF button next to Save) writes
the whole design to one PDF, one page per sheet, in each sheet's own page
size and orientation, as vector graphics. Needs the Windows "Microsoft
Print to PDF" printer (standard in Windows 10/11).

**Test:**
- [ ] Page size matches Page Setup (e.g. A3 landscape = 420 × 297 mm).
- [ ] A design with sheets of different sizes gives pages of different sizes.
- [ ] Text is sharp when zoomed in and can be selected/searched.
- [ ] Variables, Sheets N of M and colours appear correctly.
- [ ] Several exports in a row work (please report if ConCAD stops or runs
      out of memory — a known, not yet reproduced problem).

---

## 12. Groups

**What:** several objects that behave as one block — selected, moved,
rotated, copied and deleted together. Placed modules (§13) are groups too.

**How to use:**
- Select objects → **Object → Create Group** (**Ctrl+G**) or right-click.
- Click any part: the whole group is selected (dotted frame).
- **Double-click** a group (or Object → Edit Group) to edit its parts one
  by one (orange dashed frame); anything you draw meanwhile joins the
  group. **Esc**, a click outside, or Object → Finish Editing Group closes
  it.
- **Ctrl+Shift+G** (Object → Ungroup) dissolves it.

**Test:**
- [ ] Move: wires connected from outside stretch along.
- [ ] Rotate (Ctrl+R), Flip (Ctrl+F), Delete, Copy/Paste (gives a
      separate group), Undo all work on the whole group.
- [ ] Edit Group: change one part, add a wire — it becomes part of the group.
- [ ] Ungroup, then Undo: the group is back, including its wires.
- [ ] Save, reopen: still grouped.

---

## 13. Modules

**What:** a reusable piece of schematic (symbols, wires, texts…) stored in
the module library and placed like a symbol. A module can have parameters
(Name, Reference and your own fields) that texts inside it show.

**How to use:**
- **Create:** select objects → **Object → Create Module…** (or
  right-click). In the *Store Module* dialog enter the **Name** and the
  parameters **Reference**, **Partnumber**, **Type** (plus any own
  parameters with **Add**) → **Store**. The module is stored as
  **`<Type> - <Name>`** — the line at the top of the dialog shows this
  name as you type — and appears like that in the library panel, with a
  preview.
- **Place:** double-click it in the library panel; it follows the mouse;
  click to drop (right-click cancels). It is placed as a group (§12).
- **Parameters:** click a placed module: **Module Tool Options** shows
  Name, Reference, Partnumber, Type and its own parameters; click a value
  to change it, **Add** / **Delete** own parameters (the four standard ones
  stay). A text inside the module containing `{Name}`, `{Reference}`,
  `{Partnumber}`, `{Type}` or an own parameter name shows the current
  value.
- **Edit in the library:** Library → Libraries → open the module library,
  double-click (or Edit) a module: it opens in a window *Module: name*.
  Change it, **Ctrl+S** → Store.
- **Export/import:** in the library window, **File → Export all symbols…**
  and **File → Import symbols…** include modules (XML file).

**Test:**
- [ ] Store Module: an empty Name is refused; Name `Front camera` + Type
      `Camera` is stored as `Camera - Front camera`.
- [ ] Create, then place in another design: symbols draw correctly; the
      title block, grid and colours of the target design are unchanged.
- [ ] Undo removes a placed module.
- [ ] Parameters: change Reference → texts update at once; Add/Delete an
      own parameter; Name/Reference/Partnumber/Type cannot be deleted;
      Undo; save/reopen keeps values.
- [ ] Ungroup a module: texts keep the values as plain text.
- [ ] Edit a library module, store it, place it again: change is there.
      Closing the module window with unsaved changes asks first.
- [ ] Export the module library to XML and import into a new library:
      the counts reported match and the imported module places the same.

---

## 14. Keyboard shortcuts

**What:** **Ctrl+F** is now **Flip** (Find: Edit → Find…). All shortcuts
can be changed in **Options → Keyboard Shortcuts…**.

**How to use:** select a command → click in *New shortcut* → press the
keys → **Assign**. A key already in use asks before it is moved.
**Remove** clears a command's keys, **Reset All** restores the defaults.
Menus show the current keys.

**Test:**
- [ ] Assign a new key to a menu command and to a toolbar tool (e.g. Wire);
      both work and the menu shows the new key.
- [ ] Moving a key from another command asks first.
- [ ] Restart ConCAD: your keys are kept. Reset All: defaults back.

---

## 15. Copy, cut and paste of texts and labels

**What:** when a text, label, pin etc. is selected, **Ctrl+C / Ctrl+X** copy
or cut the **object**, not the text in its Tool Options field. **Ctrl+V**
pastes ConCAD objects into the drawing; plain text (e.g. from Notepad) is
still pasted into the field. **Delete** deletes the object too, as long as
you have not clicked into its text field; after placing the cursor in the
field, Delete edits the text (Backspace always edits the text).

**Test:**
- [ ] Click a free Text, Ctrl+C, Ctrl+V: a copy of the text object
      follows the mouse.
- [ ] Click a label, Ctrl+X: label removed; Ctrl+V brings it back.
- [ ] Copy words in Notepad, click a text, Ctrl+V: words go into the field.
- [ ] Click a text / label / pin / power symbol, press Delete: the object is
      deleted; Undo brings it back.
- [ ] Click a text, click into its field, press Delete: a character is
      deleted, not the object.
- [ ] Also works if Copy/Cut were given other keys (§14).

---

## 16. Autosave and crash recovery

**What:** Options → Settings → Backup saves designs with unsaved changes
every N minutes (default 10). After a crash the changes can be recovered.

- A saved design is backed up to `<name>.con.autosave` next to it; the
  backup is removed when you Save or close.
- An untitled design is backed up in `%APPDATA%\ConCAD\Recovery\`.
- Opening a design with a newer autosave asks whether to use it.
- At start-up, unsaved untitled designs from a crashed ConCAD are offered.

**Test:** (set Backup to 1 minute)
- [ ] Change a saved design, wait: `.autosave` appears; Ctrl+S: it is gone.
- [ ] Change, wait, end ConCAD in Task Manager, reopen the design: asked;
      Yes shows the changes (design marked as changed).
- [ ] An unchanged design gets no autosave file.
- [ ] New untitled design, draw, wait, kill ConCAD, start it: offered as
      *Recovered 1*.

---

## 17. Netlist, symbol references and parts list

**What:** ConCAD is for wiring/system schematics, so TinyCAD's PCB,
design-rule (ERC), SPICE and VHDL functions were removed. What remains on
the **Special** menu:
- **Generate Symbol References…** — numbers the components (U1, U2, …).
- **Create Netlist…** — writes the connections: a **TinyCAD** text file
  (the components, then each net with the ref/pins connected to it) or an
  **XML** file. Wires and cables both connect.
- **Create Parts List…** — a list of the components.

**Test:**
- [ ] Special menu has exactly: Generate Symbol References, Create
      Netlist, Create Parts List, Insert another design as symbol.
- [ ] Create Netlist: only TinyCAD and XML formats; the file extension
      follows the format; the file opens afterwards; pins joined by a
      cable or a wire are in the same net.
- [ ] Generate Symbol References and Create Parts List work as before.
- [ ] Store Symbol dialog: no SPICE tab, no automatic Package field.
- [ ] Edit → Find…: results window *Find Results* with Close only;
      clicking a result shows the item.

---

## 18. Libraries

**What:** SQLite libraries (`.TCLib`) that are read-only or locked (e.g.
under Program Files) now open instead of failing, and removing a library
no longer crashes ConCAD.

**Test:**
- [ ] Add a read-only `.TCLib`: it opens and its symbols can be placed.
- [ ] Remove a library in Library → Libraries: no error.

---

## 19. Reporting a problem: Help → Report a Bug

**What:** ConCAD keeps a log of what you do (commands, files opened and
saved, messages) and, if it crashes, saves crash information. **Help →
Report a Bug…** sends this to the developer together with your
description.

**How to use:**
1. Help → Report a Bug… (or answer Yes when ConCAD offers it after it
   did not close normally).
2. Describe what happened and what you did just before.
3. Leave the boxes ticked (logs, crash information, the open design) —
   untick the design if it must not leave your company.
4. Click **Send…**: an email opens in Outlook with the report attached;
   click Send there. Without Outlook, the report is saved on your
   Desktop and an email opens — attach the file to it.

The logs are in `%LOCALAPPDATA%\ConCAD\Logs` (kept for 14 days).

**Test:**
- [ ] Report a Bug: the email opens with the zip attached and the
      description in the text.
- [ ] End ConCAD in Task Manager and start it again: it offers a report.
