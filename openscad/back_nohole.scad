// Back shell variant: identical to back.scad, minus the second wall cutout
// (2026-08-15).
//
// Omitted feature: `cutout2` — the 8mm (along Y) x 4mm (along Z) notch in the
// right wall, flush with the mating face at z=0, spanning y=73.7-81.7. The
// matching notch in the mating lip goes away with it, so the perimeter tongue
// runs unbroken all the way around in this variant.
//
// Everything else — the port cutout, the small hole + RESET label, all four
// screw bosses/counterbores, the support spacers, the switch tower and the
// battery walls — is exactly as in back.scad.
//
// This is deliberately a wrapper, not a copy: the geometry lives in back.scad
// alone, so this variant can't drift out of sync the way the front.scad /
// back.scad duplicated values do. `use` pulls in the modules without running
// back.scad's own top-level back_shell() call.
//
// Render:  openscad -o back_nohole.stl back_nohole.scad

use <back.scad>

back_shell(with_cutout2 = false);
