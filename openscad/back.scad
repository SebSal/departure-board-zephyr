// Back/lower shell for monitor case — mates with front.scad
// Flat-cap style: no walls overlapping the front shell's outer walls, just
// butts up against its open back and is held on by two screws threading
// into the nut holders already embedded in front.scad.
//
// The values below marked "must match front.scad" are duplicated rather
// than shared, mirroring how front.scad is itself self-contained. If those
// values change in front.scad, update them here too.

// --- Configurable ---
outer_wall = 1.2;   // must match front.scad's outer_wall (increased from 0.8mm 2026-07-12 for stability)
back_wall  = 1.0;   // solid back face thickness (analogous to front_wall)
screw_d    = 3.4;   // M3 clearance hole — must match front.scad's screw_d

// --- Shared footprint (must match front.scad's outer_width / outer_height) ---
outer_width  = 50.1;
outer_height = 92.4;
device_width = 46.90;   // must match front.scad's device_width
dev_off_x    = 1.6;     // must match front.scad's outer_wall + short_clearance

// --- Depth ---
// front_depth must match front.scad's outer_depth. back_depth grew from 9mm
// to 15mm (2026-07-11, +6mm requested) to fit more of the device.
front_depth = 5.5;
back_depth  = 15;
total_depth = front_depth + back_depth;   // 20.5mm

inner_width  = outer_width  - 2 * outer_wall;
inner_height = outer_height - 2 * outer_wall;
inner_depth  = back_depth - back_wall;

// Rounded vertical outer edges (2026-07-13, cosmetic) — must match
// front.scad's corner_radius so the two shells' profiles line up. See that
// file's comment for the ~4.1mm safety margin against puncturing outer_wall.
corner_radius = 3;   // mm — must match front.scad

// Cavity corners rounded to match (2026-08-15) — must match front.scad's
// inner_corner_radius. A sharp cavity corner inside a rounded outer profile
// left only 0.45mm of wall on the diagonal instead of outer_wall's 1.2mm,
// which the slicer couldn't fill (visible gap in all four corners of both
// shells). See front.scad for the full derivation.
inner_corner_radius = corner_radius - outer_wall;   // 1.8mm

// --- Mating lip / lid mechanism (2026-07-12) ---
// A thin perimeter tongue that stands proud of the mating face (z=0) and
// pokes into front.scad's open cavity, self-aligning the two shells like a
// lid before the screws clamp them. lip_wall is a second, thicker "outer
// wall" used only to size this tongue: the 0.8mm difference from the normal
// outer_wall becomes the tongue's radial thickness. The tongue's outer edge
// sits at the normal outer_wall inset — flush against front.scad's inner
// cavity wall, since both are derived from the same outer_width/outer_height.
// lip_tolerance (2026-07-12) shrinks the tongue's outer edge inward by this
// much per side so it isn't a zero-clearance fit — same pattern as
// front.scad's screen_margin. Start small; widen if it still prints too tight.
lip_wall         = outer_wall + 0.8;   // 2.0mm — defines the tongue's inner edge
lip_height       = 1.0;                // mm the tongue protrudes past z=0 into front.scad's cavity
lip_tolerance    = 0.1;                // mm clearance per side against front.scad's cavity wall
lip_outer_width  = inner_width  - 2 * lip_tolerance;
lip_outer_height = inner_height - 2 * lip_tolerance;
lip_inner_width  = outer_width  - 2 * lip_wall;
lip_inner_height = outer_height - 2 * lip_wall;
lip_taper_height = lip_wall - outer_wall;   // 0.8mm — set equal to the ring's
                           // own width growth (lip_wall - outer_wall) for
                           // exactly a 45° taper (2026-07-12, print-
                           // friendliness) — same pattern as the button cap:
                           // printing proceeds from the back wall (early)
                           // toward the tongue (late), and the ring's
                           // ~0.8mm width would otherwise appear as a small
                           // shelf with nothing beneath it, right at the
                           // wall connection.
// lip_tolerance's outer-edge shrink (inset outer_wall+lip_tolerance = 1.3mm)
// never overlaps the perimeter wall's own footprint (inset 0-outer_wall =
// 0-1.2mm) — there's a bare 0.1mm gap between them at every z, so the
// tongue was floating, fully disconnected (caught 2026-07-12 by the user
// noticing it visually, confirmed with projection(cut=true) cross-sections
// on both sides of z=0 — see [[feedback_verify_thin_features]]). Fixed with
// a short connecting "root": a small un-shrunk (no tolerance) ring segment
// extending from z=0 up into the wall's own solid depth, so it genuinely
// overlaps the wall's footprint over a real z-range rather than just
// touching at a single z=0 edge.
lip_root_height  = 0.4;   // mm the connecting root extends into back.scad's own wall (z>0)

// Lip corner radii (2026-08-15), following the cavity's rounding above. The
// tongue has to fit inside front.scad's now-rounded cavity corner, so its
// outer boundary is rounded by the same amount less lip_tolerance — a sharp
// 1.3mm-inset corner would sit 2.40mm from the arc centre, 0.60mm outside
// the front cavity's 1.8mm corner arc, i.e. straight into the front shell's
// wall. The inner hole is rounded too, otherwise the ring's own width at the
// corner collapses from 0.7mm to 0.29mm — the same too-thin-to-slice problem
// this fix is about, just one feature further in. Both keep the ring a
// constant lip_wall - outer_wall - lip_tolerance = 0.7mm wide all the way
// around, concentric with the outer profile's arc centre.
lip_root_corner_r  = inner_corner_radius;                    // 1.8mm — root matches the cavity
lip_outer_corner_r = inner_corner_radius - lip_tolerance;    // 1.7mm — tongue, shrunk by the tolerance
lip_inner_corner_r = corner_radius - lip_wall;               // 1.0mm — the ring's inner hole

// M3x10 screw: 6mm-diameter head, actually 3mm thick, 10mm shaft.
// The counterbore is cut deeper than the screw's real head (2026-07-11):
// with back_depth increased, a plain 3mm-deep counterbore left the screw
// 3.8mm short of the nut. Recessing it further (screw head sits below
// flush, with an empty gap above it) lets the same fixed-length screw
// reach the nut — head_depth here means "counterbore depth", not the
// screw's actual head thickness.
screw_shaft_len = 10;
nut_far_face    = 3.7;    // must match front.scad: front_wall + nut_thickness + nut_tolerance
target_tip_z    = 1.5;    // nominal screw tip position — matches the ~81% nut engagement from the original fit
// Counterbore deepened (2026-08-03): screws weren't fully catching in the
// nuts on a real assembly, i.e. the head bottomed in the counterbore before
// the threads had pulled the shells together. Sinking the head deeper lets
// the same fixed-length M3x10 reach further.
// Set to exactly 0.5mm (2026-08-25, was 1mm): front.scad's nut pocket is
// backstopped by its solid 1.0mm front_wall, so the pocket floor sits at
// global z=1.0 (front outer face = 0) and the screw tip can go no deeper.
// With this value the counterbore shoulder lands at global z=11.0 (local
// back_depth - head_depth = 5.5), exactly screw_shaft_len (10mm) above that
// floor — head seating and tip contact coincide. At the previous 1mm the
// shoulder was at global 10.5, a 9.5mm gap for a 10mm shaft, so the tip
// bottomed on the front wall with the head still 0.5mm shy of the shoulder:
// the shells were clamped by the tip pressing on front_wall instead of by
// the head bearing on the boss. Going deeper than
// this achieves nothing without a relief pocket under the nut in front.scad
// (front_wall is only 1.0mm, so it would have to be blind and shallow, or
// break through to the visible front face).
screw_extra_depth = 0.5;
head_depth      = total_depth - screw_shaft_len - target_tip_z + screw_extra_depth;   // 9.5mm
head_d          = 6;

// Screw boss: the back cap alone is only 1mm thick, so the counterbore's
// step would land inside the hollow cavity with no material to seat
// against. A boss standing off the back face into the cavity gives the
// shoulder (where the screw head bears) real material on both sides.
boss_d     = 8;    // > head_d, leaves a wall around the counterbore
boss_extra = 2;    // solid material beyond the counterbore depth, forms the shoulder
boss_h     = head_depth + boss_extra;   // 11.5mm — how far the boss stands into the cavity
                                        // (still < inner_depth = 14mm, so it stays rooted
                                        //  in the back cap and clear of the mating face)

// --- Screw holes: must match front.scad's nut_positions ---
// X offset grown 35.05/15.05 -> 40.05/10.05 (2026-07-13): front.scad's nut
// holders moved 0.5cm further out (+X/-X) to free room for the widened
// front button flap. Y unchanged.
// Four screws (2026-08-03): the original diagonal pair (screw_x1, screw_y1)
// and (screw_x2, screw_y2) is now a full corner set — each one mirrored
// across X, so both the low-Y and the high-Y end get a symmetric pair. The
// two added bosses are clear of everything else in this shell: the low-Y one
// (10.05, 5.55) sits well below battery_wall1 (y=14.7) and spacer1
// (y=41.2-51.2), the high-Y one (40.05, 86.85) is ~7.9mm clear on X of the
// switch tower (x=21.9-28.2) and ~4.9mm clear on X of the right-wall
// cutouts. Same X/Y values as the existing pair — see front.scad.
screw_x1 = 40.05;
screw_y1 = 5.55;
screw_x2 = 10.05;
screw_y2 = 86.85;
screw_positions = [[screw_x1, screw_y1], [screw_x2, screw_y1],
                   [screw_x1, screw_y2], [screw_x2, screw_y2]];

// --- Port cutout ---
// 1.4cm + buffer, on the right wall, but measured from the TOP corner
// instead of the bottom (2026-07-11 — was on the wrong y-side; x/wall side
// was correct all along). dev_off_y applies equally from either end since
// long_clearance is symmetric top/bottom. Width runs along Y (up the edge),
// height runs along Z (into the depth), flush with the back face.
dev_off_y     = 10.7;   // must match front.scad's outer_wall + long_clearance
cutout_offset = 14;     // 1.4cm, pre-buffer
cutout_width  = 12;     // 1.2cm, along Y
cutout_height = 7;      // 0.7cm, along Z — flush with the back face
cutout_y      = outer_height - dev_off_y - cutout_offset - cutout_width;   // 55.7mm from the shell's y=0 edge

// --- Second cutout, near the old big cutout ---
// Begins at y=0 + dev_off_y buffer, measured from the TOP corner — same
// side/convention as the first cutout (2026-07-11 fix: was measured from
// the bottom, landing far from the old cutout instead of near it).
// 8mm wide (along Y), 4mm high (along Z) — flush with the mating face
// (z=0), not the back face like the first cutout (2026-07-11 fix).
cutout2_width  = 8;           // along Y
cutout2_height = 4;           // along Z, flush with the mating face (z=0)
cutout2_offset = 0;           // 0mm, pre-buffer
cutout2_y      = outer_height - dev_off_y - cutout2_offset - cutout2_width;   // 73.7mm from the shell's y=0 edge

// --- Small hole near the cutout ---
// 27mm from the top (mirrored same as the cutout) + dev_off_y buffer.
// 6mm in from the device's right edge (the side facing the cutout) + dev_off_x
// buffer. Both given pre-buffer.
hole_r = 1;                                    // 1mm radius
hole_y = outer_height - dev_off_y - 27 + 1;    // 55.7mm from the shell's y=0 edge
                                                // (+1mm, 2026-07-12 manual reposition —
                                                // formula's base value is 54.7mm; the old
                                                // "54.3mm" comment here was stale, left over
                                                // from before outer_wall/dev_off_y changed
                                                // earlier this session)
hole_x = (dev_off_x + device_width) - 6 + 1;   // 43.5mm from the shell's x=0 edge
                                                // (+1mm, 2026-07-12 manual reposition —
                                                // formula's base value is 42.5mm; the old
                                                // "42.1mm" comment here was likewise stale)

// --- Rectangular spacers on the x=0 and x=outer_width walls ---
// Simple solid blocks, no hole/pin engagement (unlike front.scad's spacers).
// width = X protrusion from the wall into the cavity; height = Y extent
// along the wall; depth = Z extent, matching this project's usual meaning
// of "depth". Fixed at 8mm — kept as-is when back_depth grew to 15mm
// (2026-07-11). Flush with the inner back-cap surface, stopping 6mm short
// of the mating face (z=0) rather than touching both ends.
spacer_depth = 8;

// Spacer 1: x=0 wall. Y position wasn't given, so it's centered on the wall.
spacer1_width  = 10;
spacer1_height = 10;
spacer1_y      = outer_height / 2 - spacer1_height / 2;   // centered

// Spacer 2: x=outer_width wall. 1cm + dev_off_y buffer from the bottom —
// the end away from the cutout (cutout sits at y=55.7-67.7, in the upper
// half) — not centered like spacer 1. Width/height are swapped relative to
// spacer 1: 5mm wide (X protrusion) x 10mm high (Y extent).
spacer2_width  = 5;
spacer2_height = 10;
spacer2_y      = dev_off_y + 10;   // 1cm + buffer from the shell's y=0 edge

// --- Battery alignment wall (2026-08-03) ---
// A short standing wall to align a battery glued to the inside of the back
// cap. Runs the full inner width (x direction).
// Position corrected (2026-08-03, twice): first attempt sat in the buffer
// zone before the device (y<dev_off_y), which put it right in the path of
// screw1's counterbore (radius 3mm around screw_y1=5.55mm reaches y=2.55-
// 8.55mm) — cutting a ~5.8mm gap through the wall (confirmed via an
// intersection probe). Corrected: the wall belongs INSIDE the device's own
// footprint instead (y>=dev_off_y), not in the screw/buffer zone before it
// — 4mm in from the device's near edge (dev_off_y=10.7mm, itself
// outer_wall + long_clearance from the outer wall), clear of both screw
// bosses/holes (which live entirely below dev_off_y) and spacer2 (starts
// further along at y=20.3mm).
battery_wall_thickness = 0.4;                 // mm, Y depth of each wall
battery_wall1_y = dev_off_y + 4;               // 14.7mm — 4mm inside the device footprint
// Second wall (2026-08-03): parallel to the first, 40mm in from the same
// near/bottom device edge (dev_off_y), confirmed via AskUserQuestion —
// still well inside the device's 71mm-tall footprint (10.7-81.7mm), clear
// of spacer2 (y=20.3-30.3) and the switch tower (y~82.8-90.8).
battery_wall2_y = dev_off_y + 40;              // 50.7mm

battery_wall_height = 3;                       // mm tall (Z) — grown from 2mm, 2026-08-03 post-test-print
// Flush with the inner back-cap surface, standing toward the mating face —
// same convention as back_spacer()/switch_tower() (this shell prints
// back-cap-down, so a wall rooted here needs no bridging/support).
battery_wall_z         = inner_depth - battery_wall_height;

// --- Engraved "RESET" text, beside the small hole ---
engrave_depth = 0.25;   // recessed into the back face
text_size     = 4;      // font height (mm); back to the original size (2026-07-11)
text_gap      = 2;      // gap between the hole and the text
// Text runs along Y (rotated 90°). Centered on hole_y (2026-07-11) so the
// middle letter "S" sits at the same Y as the hole, and offset to the left
// on X so it doesn't overlap the hole's circle. Rotated text's X-extent is
// ~text_size (measured ~3.93mm for text_size=4), half ~2mm.
text_y            = hole_y;                                      // "S" aligned with the hole's Y
text_half_width_x = 2;                                            // half of the rotated text's X-extent
text_x = hole_x - hole_r - text_gap - text_half_width_x;          // to the left of the hole

// Boss: solid material standing off the back face into the cavity, so the
// screw hole below has something to cut a real shoulder into.
module screw_boss() {
    translate([0, 0, back_depth - boss_h])
        cylinder(h = boss_h, d = boss_d, $fn = 32);
}

// --- Front button flap switch mount (2026-07-13) ---
// Moved here from front.scad after three failed print-friendly attempts on
// that side — a switch holder floating in the front shell's open cavity has
// no nearby solid material to root into without long unsupported bridges
// (front.scad prints front-face-down, so anything reaching sideways through
// open cavity air has nothing below it). This shell's own back cap is a
// large, solid, bed-adjacent face (see screw_boss()'s comment and the
// lip_taper_height note above — back.scad prints back-cap-down) sitting
// directly behind where the switch needs to be, so a tower standing
// straight up from it needs no bridging at all: every layer rests on the
// layer below, all the way from the cap to the pocket.
//
// Z stack (this file's own coordinates; front_depth (5.5mm) = front.scad's
// outer_depth is the shared mating-face reference, z=0 here):
//   switch_mount_z (0)       = mounting plane (legs side), at the mating face
//   switch_body_top_z (-3.5) = top of the 6x6mm body / base of the stem —
//                               the pocket's open end, extending INTO
//                               front.scad's cavity past the mating face
//   stem tip (-5.0, not printed — real hardware, not part of this model)
//   touches front.scad's flap at that file's own global z=0.5
//   (its flap_thickness) — see front.scad's switch_stem_z comment for the
//   matching math. Re-check this fit if front_depth, front.scad's
//   flap_thickness, or switch_total_h ever change on either side.
switch_cx = outer_width / 2;   // 25.05mm — must match front.scad's flap center (outer_width/2)
switch_cy = 86.8;               // must match front.scad's flap_y + flap_height/2 (82.8 + 8/2) — moved to the high-Y end 2026-07-13
switch_body        = 6.0;       // must match front.scad — body footprint (square), from button.png
switch_body_height = 3.5;       // must match front.scad — PCB-mount plane to top of housing
switch_total_h     = 5.0;       // must match front.scad — total height to top of stem
switch_tolerance   = 0.3;       // must match front.scad — total clearance around the body
switch_wall         = 1.0;      // tower wall thickness around the pocket
switch_pocket_inner = switch_body + switch_tolerance;           // 6.3mm
switch_pocket_outer = switch_pocket_inner + 2 * switch_wall;    // 8.3mm
switch_mount_z      = 0;                                        // mating face — pocket floor
switch_body_top_z   = switch_mount_z - switch_body_height;      // -3.5mm — pocket's open end
switch_tower_root_z = inner_depth + 0.3;                        // 14.3mm — 0.3mm into the solid back cap

// Legs exit through the X-facing sides of the body (2 per side, spaced
// switch_leg_sep apart in Y, per button.png's P.C.B. Land Pattern). A
// drilled notch through solid X-facing walls wasn't reliable to print/fit
// (2026-07-13), so those walls were dropped entirely instead — only the
// front/back (Y-facing) walls remain, and legs/wires get the whole open
// X side rather than a narrow slot.
switch_leg_sep = 4.5;   // mm between the two legs on each side (datasheet) — kept for reference

// Solid tower rooted in the back cap (switch_tower_root_z, genuine overlap
// — see switch_tower_root_z comment), extending straight toward the mating
// face and beyond into front.scad's cavity. Only the front/back (Y-facing)
// walls grip the switch body — the X-facing walls were removed entirely
// (2026-07-13, "two are enough to hold the button") so legs/wires exit
// through the fully open X sides with no notch needed. A solid cap above
// the mounting plane (switch_mount_z up to switch_tower_root_z) blocks the
// switch from traveling past z=0 and gives the two walls a solid root.
// No connection to front.scad's own geometry needed — the two shells only
// meet by physical contact once assembled.
module switch_tower() {
    // cap: solid above the mounting plane, spans the full Y width so the
    // walls below have solid material to root into
    translate([switch_cx - switch_pocket_inner / 2, switch_cy - switch_pocket_outer / 2, switch_mount_z])
        cube([switch_pocket_inner, switch_pocket_outer, switch_tower_root_z - switch_mount_z]);
    // front/back walls gripping the body, open on the X sides for legs/wires
    for (wall_y = [switch_cy - switch_pocket_outer / 2, switch_cy + switch_pocket_inner / 2])
        translate([switch_cx - switch_pocket_inner / 2, wall_y, switch_body_top_z])
            cube([switch_pocket_inner, switch_wall, switch_mount_z - switch_body_top_z]);
}

// Perimeter tongue for the mating lip: a short connecting root (z=0 to
// +lip_root_height, un-shrunk footprint, genuinely overlapping the
// perimeter wall's own material) plus the tongue itself, which extrudes
// downward from z=0 into negative z (past the mating face, into
// front.scad's cavity) using the tolerance-shrunk footprint. The outer
// boundary is constant throughout (it's what connects to the wall); the
// inner hole tapers (2026-07-12) so the ring grows from a thin sliver at
// the wall connection to full width, instead of bursting to full width in
// a single layer right where it starts — see lip_taper_height's comment.
module mating_lip() {
    difference() {
        union() {
            // root outer boundary (constant, matches the wall)
            linear_extrude(height = lip_root_height)
                translate([outer_wall, outer_wall])
                    rounded_rect(inner_width, inner_height, lip_root_corner_r);
            // tongue outer boundary (constant, tolerance-shrunk)
            translate([0, 0, -lip_height])
                linear_extrude(height = lip_height)
                    translate([outer_wall + lip_tolerance, outer_wall + lip_tolerance])
                        rounded_rect(lip_outer_width, lip_outer_height, lip_outer_corner_r);
        }
        union() {
            // tapered inner hole: grows from a thin ring (near the wall
            // connection, z=lip_root_height) to the full lip_inner size
            // over lip_taper_height
            hull() {
                translate([0, 0, lip_root_height - 0.001])
                    linear_extrude(height = 0.001)
                        translate([outer_wall + 0.1, outer_wall + 0.1])
                            rounded_rect(inner_width - 0.2, inner_height - 0.2,
                                         inner_corner_radius - 0.1);
                translate([0, 0, lip_root_height - lip_taper_height])
                    linear_extrude(height = 0.001)
                        translate([lip_wall, lip_wall])
                            rounded_rect(lip_inner_width, lip_inner_height, lip_inner_corner_r);
            }
            // constant inner hole below the taper, through the rest of the
            // root and the whole tongue
            translate([0, 0, -lip_height - 0.1])
                linear_extrude(height = lip_height + lip_root_height - lip_taper_height + 0.1)
                    translate([lip_wall, lip_wall])
                        rounded_rect(lip_inner_width, lip_inner_height, lip_inner_corner_r);
        }
    }
}

// Rectangular support spacer standing off a side wall into the cavity.
// from_right=true attaches it to the x=outer_width wall instead of x=0.
// Flush with the inner back-cap surface (2026-07-11), not the mating face.
module back_spacer(y, width, height, from_right = false) {
    x = from_right ? outer_width - outer_wall - width : outer_wall;
    translate([x, y, inner_depth - spacer_depth])
        cube([width, height, spacer_depth]);
}

// Battery alignment wall: full inner width, standing 2mm off the inner
// back-cap surface, inside the device's footprint. y is the wall's near
// (translate) edge — battery_wall1_y / battery_wall2_y.
module battery_wall(y) {
    translate([outer_wall, y, battery_wall_z])
        cube([inner_width, battery_wall_thickness, battery_wall_height]);
}

// "RESET" engraved into the back face, beside the hole — the "S" (middle
// letter) shares the hole's Y coordinate, offset left on X (2026-07-11).
module reset_text() {
    translate([text_x, text_y, back_depth - engrave_depth])
        rotate([0, 0, 270])
            linear_extrude(height = engrave_depth + 0.1)
                text("RESET", size = text_size, halign = "center", valign = "center",
                     font = "Liberation Sans:style=Bold");
}

// Screw clearance hole with a counterbore that seats the screw head flush
// in the back face. Shaft clearance runs the full depth so it reaches
// through to the nut in front.scad. Cut through the boss above, the step
// from head_d down to screw_d lands inside solid boss material — that's
// the shoulder the screw head bears against when tightened.
module screw_hole() {
    translate([0, 0, -0.1])
        cylinder(h = back_depth + 0.2, d = screw_d, $fn = 32);
    translate([0, 0, back_depth - head_depth])
        cylinder(h = head_depth + 0.1, d = head_d, $fn = 48);
}

// 2D rounded-rectangle footprint, extruded to make the outer solid
// (duplicated from front.scad — see this project's sync convention)
module rounded_rect(w, h, r) {
    hull() {
        for (x = [r, w - r])
            for (y = [r, h - r])
                translate([x, y]) circle(r = r, $fn = 32);
    }
}

// with_cutout2 (2026-08-15): set false for a variant of this shell that keeps
// everything else but omits the second (mating-face) wall cutout — both the
// notch itself and the matching notch in the mating lip, so the tongue runs
// unbroken around the full perimeter. See back_nohole.scad, which is a thin
// wrapper around this module rather than a copy of this file.
module back_shell(with_cutout2 = true) {
    difference() {
        union() {
            difference() {
                linear_extrude(height = back_depth) rounded_rect(outer_width, outer_height, corner_radius);
                // hollow interior, open at z=0 (mates with front shell's open
                // back) — corners rounded by inner_corner_radius so the
                // perimeter wall keeps its full outer_wall thickness there
                translate([outer_wall, outer_wall, 0])
                    linear_extrude(height = inner_depth)
                        rounded_rect(inner_width, inner_height, inner_corner_radius);
            }
            // bosses, added back into the cavity so the screw holes below have a shoulder
            for (pos = screw_positions)
                translate([pos[0], pos[1], 0]) screw_boss();
            // support spacers on the side walls
            back_spacer(spacer1_y, spacer1_width, spacer1_height);
            back_spacer(spacer2_y, spacer2_width, spacer2_height, from_right = true);
            // mating lip: perimeter tongue poking into front.scad's cavity
            mating_lip();
            // switch mount: holds the tactile switch behind front.scad's button flap
            switch_tower();
            // battery alignment walls, inside the device footprint
            battery_wall(battery_wall1_y);
            battery_wall(battery_wall2_y);
        }
        // screw clearance + head counterbore, cut through the bosses above
        for (pos = screw_positions)
            translate([pos[0], pos[1], 0])
                screw_hole();
        // port cutout: notch through the right wall, flush with the back face
        translate([outer_width - outer_wall - 0.1, cutout_y, back_depth - cutout_height])
            cube([outer_wall + 0.2, cutout_width, cutout_height + 0.1]);
        if (with_cutout2) {
            // second cutout, near the port cutout, flush with the mating face (z=0)
            translate([outer_width - outer_wall - 0.1, cutout2_y, -0.1])
                cube([outer_wall + 0.2, cutout2_width, cutout2_height + 0.1]);
            // remove the mating lip where it would sit over the 2nd cutout, so
            // the opening isn't blocked by the tongue (2026-07-12). Spans the
            // tongue (negative z) AND the connecting root (0 to lip_root_height)
            // — the root's addition (2026-07-12 lip-floating fix) reintroduced
            // coverage here that this cut wasn't reaching, since it originally
            // only extended to z=0.1.
            translate([outer_width - lip_wall - 0.1, cutout2_y, -lip_height - 0.1])
                cube([lip_wall + 0.2, cutout2_width, lip_height + lip_root_height + 0.2]);
        }
        // small hole near the cutout, straight through
        translate([hole_x, hole_y, -0.1])
            cylinder(h = back_depth + 0.2, d = hole_r * 2, $fn = 32);
        // engraved "RESET" label, underneath the small hole
        reset_text();
    }
}

back_shell();
