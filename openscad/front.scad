// Front/upper shell for monitor case
// Screen: 2.40" TFT SMT024-CR-SP8p_LCM

// --- Configurable ---
front_wall      = 1.0;   // front face thickness (mm)
outer_wall      = 1.2;   // perimeter wall thickness (mm) (increased from 0.8mm on 2026-07-12 for stability)
long_clearance  = 9.5;   // space above and below device (longer axis) (mm)
                          // enlarged from 5.0mm to fit M3 nut holders (needs boss_d + margin ≈ 9.1mm)
short_clearance = 0.4;   // clearance on each side of device (shorter axis) (mm)

// --- Device measurements ---
device_width  = 46.90;
device_height = 71.00;

// --- Screen bezel cutout ---
screen_width  = 43.50;
screen_height = 61.10;
screen_margin = 0.2;   // printer isn't precise enough — extra clearance per side (2026-07-11, reduced from 0.4mm on 2026-07-12)

// --- Display PCB mounting holes ---
// 2.6mm from long (top/bottom) edges; 37.7mm apart on short axis
// → (46.9 - 37.7) / 2 = 4.6mm from each short (left/right) edge
hole_d    = 2.0;
hole_sep  = 37.7;   // center-to-center distance along short axis
hole_x1   = (device_width  - hole_sep)  / 2;   // 4.60mm from PCB left
hole_x2   =  device_width  - hole_x1;           // 42.30mm from PCB left
hole_y1   = 2.6;                                 // from PCB bottom
hole_y2   = device_height - hole_y1;             // from PCB bottom

// --- Spacer geometry ---
// Each spacer is a stepped peg:
//   shoulder: wide base the PCB front face rests on
//   pin:      narrow peg that registers into the PCB mounting hole
display_bezel_rise = 3.5;
pcb_thickness      = 1.2;

shoulder_h = display_bezel_rise - front_wall;
shoulder_d = 4.0;
pin_h      = pcb_thickness + 0.5 + 0.5;   // 2.2mm — pin (the thinner part of
                                           // the spacer) extended 0.5mm (2026-07-12)
pin_d      = hole_d - 0.2;   // 1.8mm — snug fit in 2mm hole

// --- M3 nut trap geometry ---
// Back to the original simple design (2026-07-11): the side-loading slot
// version had unprintable overhangs. This is just a hex pocket, open at
// the back — front wall is the backstop. Nut is glued in by hand, no screw
// hole or slide-in mechanism needed.
nut_across_flats = 5.5;   // M3 hex nut, nominal width across flats
nut_thickness    = 2.4;   // M3 hex nut, nominal thickness
nut_tolerance    = 0.3;   // extra clearance so a real nut fits into the printed pocket
nut_wall         = 1.0;   // plastic wall around the hex pocket

nut_pocket_af    = nut_across_flats + nut_tolerance;   // pocket flat-to-flat
nut_pocket_depth = nut_thickness + nut_tolerance;      // pocket depth (Z), backstopped by front_wall
nut_pocket_ac    = nut_pocket_af / cos(30);            // pocket corner-to-corner (cylinder d for $fn=6)
nut_boss_d       = nut_pocket_ac + 2 * nut_wall;       // outer boss diameter

// --- Derived ---
outer_width  = device_width  + 2 * short_clearance + 2 * outer_wall;
outer_height = device_height + 2 * long_clearance  + 2 * outer_wall;
outer_depth  = 5.5;  // shrunk from 10mm (2026-07-11) to remove dead space beyond the
                      // spacer pins/nut bosses, so the back shell's M3x10 screws can
                      // reach the nut with real thread engagement — see back.scad

inner_width  = outer_width  - 2 * outer_wall;
inner_height = outer_height - 2 * outer_wall;
inner_depth  = outer_depth  - front_wall;

// Rounded vertical outer edges (2026-07-13, cosmetic). Kept comfortably
// below the ~4.1mm radius that would cut through outer_wall (1.2mm) at the
// diagonal and puncture into the cavity (r < outer_wall * sqrt(2) /
// (sqrt(2) - 1) ≈ 4.1mm).
corner_radius = 3;   // mm — must match back.scad so the two shells' profiles align

// The cavity is rounded to match (2026-08-15). It used to stay a sharp
// rectangle, which left the corners far thinner than outer_wall: the outer
// arc's centre sits at (corner_radius, corner_radius) while the sharp cavity
// corner sat at (outer_wall, outer_wall), only sqrt(2)*(3-1.2)=2.55mm away,
// so the diagonal wall thickness there was 3-2.55 = 0.45mm — under two
// extrusion widths, which the slicer couldn't fill, leaving a visible gap in
// all four corners of both shells. Rounding the cavity by the same amount
// less outer_wall keeps the wall a constant 1.2mm the whole way around.
// Only the corner squares (x or y within corner_radius of an edge) change,
// and nothing lives there — the nearest features are the nut bosses
// (x >= 5.7) and the device itself (y >= 10.7).
inner_corner_radius = corner_radius - outer_wall;   // 1.8mm

// Device sits in inner cavity with short_clearance on left/right, long_clearance on top/bottom
dev_off_x = outer_wall + short_clearance;
dev_off_y = outer_wall + long_clearance;

// Spacer positions in shell coordinates
sp_x1 = dev_off_x + hole_x1;
sp_x2 = dev_off_x + hole_x2;
sp_y1 = dev_off_y + hole_y1;
sp_y2 = dev_off_y + hole_y2;

// Screen hole centered on the device face
screen_x = dev_off_x + (device_width  - screen_width)  / 2;
screen_y = dev_off_y + (device_height - screen_height) / 2;

// Nut holder positions: flush against the inner top/bottom wall, shifted
// sideways off-center by ±15mm.
// Offset grown from 1cm to 1.5cm (2026-07-13, "move 0.5cm up/down towards
// the side walls" — this file's own "up"/"down" convention is +X/-X) to
// free up X-direction room for the widened front button flap below.
// Four screws (2026-08-03): was a 2-screw diagonal pair — (nut_x1, nut_y1)
// and (nut_x2, nut_y2) — extended to all four corners of the ±15mm / top-
// bottom grid by mirroring each one across X. The two added positions,
// (nut_x2, nut_y1) and (nut_x1, nut_y2), reuse the exact same X/Y values, so
// each end of the case now carries a symmetric pair. The added high-Y holder
// keeps the same 1.65mm clearance to the button flap that its mirror image
// already had (the flap is centered on X, the offset is symmetric), and the
// added low-Y holder clears the nearest display spacer (sp_x1, sp_y1) by
// 2.3mm edge-to-edge. Must match back.scad's screw_positions.
nut_x1 = outer_width / 2 + 15;   // shifted +X ("up")
nut_x2 = outer_width / 2 - 15;   // shifted -X ("down")
nut_y1 = outer_wall + nut_boss_d / 2;                     // low-Y end
nut_y2 = outer_height - outer_wall - nut_boss_d / 2;      // high-Y end
nut_positions = [[nut_x1, nut_y1], [nut_x2, nut_y1],
                 [nut_x1, nut_y2], [nut_x2, nut_y2]];

// --- Front button flap (2026-07-13) ---
// A flex panel in the front face, in the high-Y end of the long_clearance
// buffer zone (same end as nut_holder 2 — moved here 2026-07-13 from the
// low-Y end/nut_holder 1 side, "button is on the wrong side"). Attached
// (uncut) at its left and right X edges; freed top/bottom by horizontal
// slit cuts through the full front-wall thickness, so pressing the middle
// flexes it inward (fixed-both-ends beam, not a living hinge — no thin
// hinge web needed). Centered on X (unlike the nut holder, which is offset)
// — the nut holders' ±15mm X offset is itself symmetric, so the flap keeps
// the same 1.65mm clearance to whichever nut boss shares its Y end.
flap_width       = 18;     // X footprint — widened from 8mm using the room freed by
                            // moving nut_holder1 out to +15 (was +10); right edge grows
                            // by the same 5mm the nut holder moved, keeping the same
                            // 1.65mm clearance to its boss that the original 8mm-wide
                            // flap had against the original nut position
flap_height      = 8;      // Y footprint
flap_x           = outer_width / 2 - flap_width / 2;      // 16.05mm, centered on outer_width
flap_band        = 1.0;    // solid left/right margin left uncut (the attachment zones) — rotated
                            // 90° (2026-07-13) from the original top/bottom-attached design
flap_slit_width  = 0.4;    // cut gap on top/bottom, matches this project's slot_gap convention
flap_slit_length = flap_width - 2 * flap_band;              // 16mm — X extent of the top/bottom cuts
// flap_y: mirrored to the high-Y end (2026-07-13) — top slit's far edge
// lands exactly at outer_height - outer_wall (91.2mm), flush with the solid
// side-wall corner there, so the cut never bites into it and leaves no
// unsupported overhang above the cut (same reasoning as the original
// low-Y placement, just flipped). Bottom slit's near edge lands at 82.4mm,
// still 0.7mm clear of the device's top edge (dev_off_y + device_height =
// 81.7mm) — no size reduction needed to make this fit.
flap_y           = outer_height - outer_wall - flap_slit_width - flap_height;  // 82.8mm
// Flush with the front face at z=0 (2026-07-13, dropped the raised protrusion
// cap for easier printing — the flap is now just the existing front_wall
// material with slits cut through it, no separate proud piece to bridge).
flap_thickness   = 0.5;    // mm — the button area is thinned to this (vs the normal
                            // 1.0mm front_wall) to make it easier to flex. First pass,
                            // "let's start with 0.5mm" (2026-07-13) — tune after a test
                            // print. Thinned from the back (cavity side), so the outer
                            // z=0 face stays flush with the rest of the front face.
flap_mark_d      = 5;      // mm — diameter of the engraved circle marking the button,
                            // centered on the flap, clear of the flap_height (8mm)
flap_mark_line_w = 0.4;    // mm — width of the engraved ring line itself (not a filled
                            // disc), matches this project's flap_slit_width convention
flap_mark_depth  = 0.25;   // mm — recessed into the outer (z=0) face, same convention
                            // as back.scad's engraved RESET text

// Note (2026-07-13): a tactile switch used to be mounted here directly (a
// collar holding it behind the flap), but three attempts all failed to be
// print-friendly — this file prints front-face-down, so anything reaching
// into the open cavity behind the flap has no nearby solid material to
// root into without long unsupported bridges. Moved to back.scad instead:
// see switch_tower() there, which roots in that shell's own back cap (a
// large solid bed-adjacent face directly behind this position) and needs
// no bridging at all. The flap itself is unchanged — it's still just a
// pressable flex panel; the switch reaches it from the assembled back shell.

// Stepped peg: shoulder (PCB rests on it) + pin (goes into PCB hole)
module spacer() {
    cylinder(h = shoulder_h, d = shoulder_d, $fn = 32);
    translate([0, 0, shoulder_h])
        cylinder(h = pin_h, d = pin_d, $fn = 32);
}

module spacers() {
    for (pos = [[sp_x1, sp_y1], [sp_x2, sp_y1], [sp_x1, sp_y2], [sp_x2, sp_y2]]) {
        translate([pos[0], pos[1], front_wall])
            spacer();
    }
}

// Captive M3 nut holder: boss with a hex pocket, backstopped by the front
// wall, open at the back — nut is glued in by hand.
module nut_holder() {
    difference() {
        cylinder(h = nut_pocket_depth, d = nut_boss_d, $fn = 32);
        cylinder(h = nut_pocket_depth, d = nut_pocket_ac, $fn = 6);
    }
}

module nut_holders() {
    for (pos = nut_positions)
        translate([pos[0], pos[1], front_wall])
            nut_holder();
}

// Frees the flap on its top and bottom sides with full-depth slit cuts
// through the front wall, leaving flap_band-wide solid margins on the left
// and right of the flap's own footprint as the attachment zones (rotated
// 90° from the original design, 2026-07-13).
module front_flap_cuts() {
    // bottom
    translate([flap_x + flap_band, flap_y - flap_slit_width, -0.1])
        cube([flap_slit_length, flap_slit_width, front_wall + 0.2]);
    // top
    translate([flap_x + flap_band, flap_y + flap_height, -0.1])
        cube([flap_slit_length, flap_slit_width, front_wall + 0.2]);
}

// Thins the whole button area (its full footprint, including the left/right
// attachment bands) down to flap_thickness, cutting away the extra material
// from the back (cavity side) only — the outer z=0 face is untouched, so it
// stays flush with the surrounding front face. Extends 0.1mm past front_wall
// to genuinely overlap the existing cavity cut, not just touch it at a
// shared face.
module front_flap_thin() {
    translate([flap_x, flap_y, flap_thickness])
        cube([flap_width, flap_height, front_wall - flap_thickness + 0.1]);
}

// Engraved circle outline (a ring, not a filled disc) on the outer face,
// centered on the flap, so the button is visually/tactilely recognizable.
// Cut from z=0 down to flap_mark_depth only — well short of flap_thickness
// (0.5mm), so it doesn't weaken the flex zone.
module front_flap_mark() {
    translate([flap_x + flap_width / 2, flap_y + flap_height / 2, -0.1])
        difference() {
            cylinder(h = flap_mark_depth + 0.1, d = flap_mark_d, $fn = 64);
            cylinder(h = flap_mark_depth + 0.2, d = flap_mark_d - 2 * flap_mark_line_w, $fn = 64);
        }
}

// 2D rounded-rectangle footprint, extruded to make the outer solid
module rounded_rect(w, h, r) {
    hull() {
        for (x = [r, w - r])
            for (y = [r, h - r])
                translate([x, y]) circle(r = r, $fn = 32);
    }
}

module front_shell() {
    union() {
        difference() {
            linear_extrude(height = outer_depth) rounded_rect(outer_width, outer_height, corner_radius);
            // hollow interior, open at back — corners rounded by
            // inner_corner_radius so the perimeter wall keeps its full
            // outer_wall thickness through the corners
            translate([outer_wall, outer_wall, front_wall])
                linear_extrude(height = inner_depth)
                    rounded_rect(inner_width, inner_height, inner_corner_radius);
            // screen cutout through front face, enlarged by screen_margin per side
            translate([screen_x - screen_margin, screen_y - screen_margin, 0])
                cube([screen_width + 2 * screen_margin, screen_height + 2 * screen_margin, front_wall]);
            // free the button flap on its top/bottom sides
            front_flap_cuts();
            // thin the button area for easier flexing
            front_flap_thin();
            // engrave a circle marking the button
            front_flap_mark();
        }
        spacers();
        nut_holders();
    }
}

front_shell();
