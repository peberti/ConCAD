# ConCAD logo / icon source

The ConCAD mark is a **bus (the vehicle)** riding on a schematic **bus bar** (a
thick wire with junction dots), with short wire "taps" off the roof — a visual
pun on the two meanings of *bus* in a schematic-capture program.

| File | What it is |
|------|------------|
| `concad-logo.svg`       | Editable vector master (detailed variant). Edit this first. |
| `concad-logo.png`       | 512 px preview of the detailed variant. |
| `concad-logo-small.png` | 512 px preview of the bold, low-detail variant used at 16/32 px. |
| `make_icon.py`          | Renders `../src/res/idr_main.ico` (16/32/48/64/128/256) and the PNG previews. |

## Regenerating the icon

```sh
python3 art/make_icon.py      # requires Pillow
```

This overwrites `src/res/idr_main.ico`, which `src/TinyCad.rc` uses for both
`IDR_MAINFRAME` (app window / taskbar) and `IDR_TCADTYPE` (the `.dsn` file icon).
The small sizes (≤32 px) use a simplified, higher-contrast drawing so the bus
silhouette stays legible; 48 px and up use the detailed art with the roof taps.

No SVG rasteriser was available when this was authored, so `make_icon.py` draws
the icon with Pillow primitives kept in sync with the SVG by hand. If you change
`concad-logo.svg`, mirror the change in `make_icon.py` (or rasterise the SVG
directly once a tool such as Inkscape / rsvg-convert is available).

The previous (upstream TinyCAD) icon remains in git history if it is ever needed:
`git checkout <old-rev> -- src/res/idr_main.ico`.
