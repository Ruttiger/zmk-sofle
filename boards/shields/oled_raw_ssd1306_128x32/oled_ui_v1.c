/*
 * Static UI V1 content for the validated 32x128 LEFT viewport.
 *
 * SPDX-License-Identifier: MIT
 */

#include "oled_gfx_portrait.h"
#include "oled_screen_content.h"

static void oled_ui_v1_draw(void)
{
	/* Five glyphs at x=1 occupy pixels 1..29. */
	oled_gfx_draw_text(1, 0, "SOFLE", true);
	oled_gfx_draw_text(1, 16, "-----", true);
	oled_gfx_draw_text(1, 36, "UI V1", true);

	/* Four glyphs occupy 23 pixels; three glyphs occupy 17 pixels. */
	oled_gfx_draw_text(4, 60, "LEFT", true);
	oled_gfx_draw_text(7, 92, "32X", true);
	oled_gfx_draw_text(7, 121, "128", true);
}

void oled_screen_content_draw(void)
{
	oled_ui_v1_draw();
}

const char *oled_screen_content_name(void)
{
	return "static OLED UI 32x128 V1";
}
