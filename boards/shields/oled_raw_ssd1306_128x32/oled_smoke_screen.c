/*
 * Minimal production-baseline smoke screen for the validated 32x128 viewport.
 *
 * SPDX-License-Identifier: MIT
 */

#include "oled_gfx_portrait.h"
#include "oled_smoke_screen.h"

void oled_smoke_screen_draw(void)
{
	oled_gfx_clear();

	/* Top-left: L marker, touching x=0 and y=0. */
	oled_gfx_hline(0, 5, 0, true);
	oled_gfx_vline(0, 0, 5, true);

	/* Top-right: filled marker, touching x=31 and y=0. */
	oled_gfx_fill_rect(29, 0, 3, 3, true);

	/* Bottom-left: outline marker, touching x=0 and y=127. */
	oled_gfx_rect(0, 123, 5, 5, true);

	/* Bottom-right: T marker, touching x=31 and y=127. */
	oled_gfx_hline(26, 31, 127, true);
	oled_gfx_vline(29, 122, 127, true);
	oled_gfx_set_pixel(31, 124, true);

	/* Short line at the logical center of the portrait viewport. */
	oled_gfx_hline(11, 20, 64, true);
}
