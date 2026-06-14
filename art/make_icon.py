#!/usr/bin/env python3
"""
Generate the ConCAD application logo / icon.

Concept: a stylized BUS (the vehicle) riding on a schematic BUS BAR (a thick
wire with junction dots), with little wire "taps" coming off the roof - fusing
the two meanings of the word "bus" for a schematic-capture program.

No SVG rasteriser is available in this environment, so the icon is drawn with
Pillow primitives.  art/concad-logo.svg is the editable vector master and is
kept visually in sync with this script by hand.

Outputs:
  art/concad-logo.png   - 512px preview
  src/res/idr_main.ico  - multi-size Windows icon (16/32/48/64/256)
"""
import os
from PIL import Image, ImageDraw

# ---- palette -------------------------------------------------------------
NAVY    = (15, 32, 64, 255)      # background (matches the splash screen)
NAVY_2  = (10, 22, 46, 255)      # deeper navy for the beltline
YELLOW  = (245, 184, 28, 255)    # bus body
YELLOW_D= (210, 150, 10, 255)    # bus shadow / hub
CYAN    = (130, 205, 255, 255)   # wires / windows accent
WINDOW  = (190, 228, 255, 255)   # glass
WHITE   = (240, 246, 255, 255)   # bus bar
TIRE    = (22, 22, 30, 255)      # wheels

REF = 256          # design grid
SS  = 8            # supersample factor (draw at REF*SS, downscale)


def rr(d, box, radius, **kw):
    d.rounded_rectangle(box, radius=radius, **kw)


def draw_logo(scale):
    """Draw the logo on a (REF*scale) square RGBA canvas."""
    n = REF * scale
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = scale  # shorthand: multiply every REF-coordinate by s

    def S(*vals):
        return [v * s for v in vals]

    # --- rounded-square background -------------------------------------
    rr(d, S(2, 2, 254, 254), radius=52 * s, fill=NAVY)

    # --- schematic bus bar (the "road") + junction dots ----------------
    bar_y = 198
    d.line(S(26, bar_y, 230, bar_y), fill=WHITE, width=int(9 * s))
    # end pins + centre node (schematic junctions)
    for cx in (40, 128, 216):
        r = 8 * s
        d.ellipse([cx * s - r, bar_y * s - r, cx * s + r, bar_y * s + r], fill=CYAN)

    # --- roof "tap" wires with junction dots (bus connections) ---------
    for cx in (84, 128, 172):
        d.line(S(cx, 96, cx, 72), fill=CYAN, width=int(4 * s))
        r = 6 * s
        d.ellipse([cx * s - r, 70 * s - r, cx * s + r, 70 * s + r], fill=CYAN)

    # --- bus body ------------------------------------------------------
    rr(d, S(36, 96, 220, 186), radius=22 * s, fill=YELLOW)
    # soft lower shadow band
    rr(d, S(36, 168, 220, 186), radius=10 * s, fill=YELLOW_D)
    rr(d, S(36, 96, 220, 172), radius=22 * s, fill=YELLOW)

    # --- windows -------------------------------------------------------
    wy0, wy1 = 110, 140
    xs = [48, 89, 130, 171]
    for x in xs:
        rr(d, S(x, wy0, x + 33, wy1), radius=6 * s, fill=WINDOW)

    # beltline stripe under the windows
    rr(d, S(40, 146, 216, 153), radius=4 * s, fill=NAVY_2)

    # door (front, right) as a thin glass panel
    rr(d, S(196, 110, 212, 168), radius=5 * s, fill=WINDOW)

    # headlight drawn as a schematic node (ring + dot)
    hx, hy, hr = 210, 162, 7
    d.ellipse(S(hx - hr, hy - hr, hx + hr, hy + hr), fill=WHITE)
    d.ellipse(S(hx - 3, hy - 3, hx + 3, hy + 3), fill=NAVY)

    # --- wheels (sitting on the bus bar) -------------------------------
    for cx in (84, 172):
        tr = 25 * s
        d.ellipse([cx * s - tr, bar_y * s - tr, cx * s + tr, bar_y * s + tr], fill=TIRE)
        hr = 10 * s
        d.ellipse([cx * s - hr, bar_y * s - hr, cx * s + hr, bar_y * s + hr], fill=YELLOW)
        cr = 3 * s
        d.ellipse([cx * s - cr, bar_y * s - cr, cx * s + cr, bar_y * s + cr], fill=NAVY)

    return img


def draw_logo_simple(scale):
    """A bolder, low-detail variant for tiny sizes (16/32 px) where the thin
    roof taps and bar dots would just turn to mud.  Bus fills more of the frame
    and only the strongest schematic cue (the bus bar + two nodes) is kept."""
    n = REF * scale
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = scale

    def S(*vals):
        return [v * s for v in vals]

    rr(d, S(2, 2, 254, 254), radius=48 * s, fill=NAVY)

    # bus bar + two nodes
    bar_y = 212
    d.line(S(20, bar_y, 236, bar_y), fill=WHITE, width=int(12 * s))
    for cx in (44, 212):
        r = 9 * s
        d.ellipse([cx * s - r, bar_y * s - r, cx * s + r, bar_y * s + r], fill=CYAN)

    # big bus body
    rr(d, S(26, 78, 230, 200), radius=26 * s, fill=YELLOW)
    rr(d, S(26, 176, 230, 200), radius=14 * s, fill=YELLOW_D)
    rr(d, S(26, 78, 230, 182), radius=26 * s, fill=YELLOW)

    # three large windows
    for x in (40, 104, 168):
        rr(d, S(x, 98, x + 50, 150), radius=8 * s, fill=WINDOW)
    rr(d, S(40, 158, 216, 168), radius=4 * s, fill=NAVY_2)

    # wheels
    for cx in (80, 176):
        tr = 30 * s
        d.ellipse([cx * s - tr, bar_y * s - tr, cx * s + tr, bar_y * s + tr], fill=TIRE)
        hr = 12 * s
        d.ellipse([cx * s - hr, bar_y * s - hr, cx * s + hr, bar_y * s + hr], fill=YELLOW)
    return img


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(here)

    master = draw_logo(SS).resize((REF, REF), Image.LANCZOS)
    simple = draw_logo_simple(SS).resize((REF, REF), Image.LANCZOS)

    # preview (detailed master)
    preview = draw_logo(2).resize((512, 512), Image.LANCZOS)
    preview.save(os.path.join(here, "concad-logo.png"))
    print("wrote concad-logo.png")
    draw_logo_simple(2).resize((512, 512), Image.LANCZOS).save(
        os.path.join(here, "concad-logo-small.png"))
    print("wrote concad-logo-small.png")

    # multi-size .ico: simplified art for <=32 px, detailed art for >=48 px.
    plan = [(16, simple), (32, simple), (48, master),
            (64, master), (128, master), (256, master)]
    frames = [src.resize((sz, sz), Image.LANCZOS) for sz, src in plan]
    ico_path = os.path.join(repo, "src", "res", "idr_main.ico")
    frames[-1].save(ico_path, format="ICO",
                    sizes=[(sz, sz) for sz, _ in plan],
                    append_images=frames[:-1])
    print("wrote", ico_path)


if __name__ == "__main__":
    main()
