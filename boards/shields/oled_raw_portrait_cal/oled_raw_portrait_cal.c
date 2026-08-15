/*
 * Portrait geometry calibration on the validated RAW SSD1306 backend.
 *
 * No font, text, widgets, or ZMK display integration is used here.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(oled_raw_portrait_cal, CONFIG_DISPLAY_LOG_LEVEL);

#define PHYSICAL_WIDTH 128U
#define PHYSICAL_HEIGHT 64U
#define PHYSICAL_PAGES (PHYSICAL_HEIGHT / 8U)
#define PHYSICAL_BUFFER_SIZE (PHYSICAL_WIDTH * PHYSICAL_PAGES)

#define PORTRAIT_WIDTH 64U
#define PORTRAIT_HEIGHT 128U

static const char oled_portrait_build_marker[] =
	"RUTTIGER_RAW_PORTRAIT_CAL_V1";
static const struct device *const oled_portrait_display =
	DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static uint8_t oled_portrait_framebuffer[PHYSICAL_BUFFER_SIZE];
static uint8_t oled_portrait_clear_buffer[PHYSICAL_BUFFER_SIZE];

static const struct display_buffer_descriptor oled_portrait_buffer_descriptor = {
	.buf_size = sizeof(oled_portrait_framebuffer),
	.width = PHYSICAL_WIDTH,
	.height = PHYSICAL_HEIGHT,
	.pitch = PHYSICAL_WIDTH,
};

/* Zephyr v0.3 has no public display_clear() operation. */
static int oled_portrait_clear(const struct device *display)
{
	memset(oled_portrait_clear_buffer, 0,
	       sizeof(oled_portrait_clear_buffer));

	return display_write(display, 0, 0, &oled_portrait_buffer_descriptor,
			    oled_portrait_clear_buffer);
}

static void portrait_set_pixel(const struct display_capabilities *caps,
			       int logical_x, int logical_y, bool on)
{
	int physical_x;
	int physical_y;
	size_t index;
	uint8_t bit;

	if (logical_x < 0 || logical_x >= (int)PORTRAIT_WIDTH ||
	    logical_y < 0 || logical_y >= (int)PORTRAIT_HEIGHT) {
		return;
	}

	/* Portrait (x,y) -> SSD1306 physical (127-y,x). */
	physical_x = (int)(PHYSICAL_WIDTH - 1U) - logical_y;
	physical_y = logical_x;

	if (physical_x < 0 || physical_x >= (int)PHYSICAL_WIDTH ||
	    physical_y < 0 || physical_y >= (int)PHYSICAL_HEIGHT) {
		return;
	}

	index = ((size_t)physical_y / 8U) * PHYSICAL_WIDTH +
		(size_t)physical_x;
	bit = (caps->screen_info & SCREEN_INFO_MONO_MSB_FIRST) ?
		(7U - (physical_y & 7U)) : (physical_y & 7U);

	/* MONO10 is 1=black, 0=white; inversion-on makes this visible. */
	if (on) {
		oled_portrait_framebuffer[index] &= (uint8_t)~BIT(bit);
	} else {
		oled_portrait_framebuffer[index] |= BIT(bit);
	}
}

static void portrait_hline(const struct display_capabilities *caps,
			   int x0, int x1, int y)
{
	if (x0 > x1) {
		int tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	for (int x = x0; x <= x1; ++x) {
		portrait_set_pixel(caps, x, y, true);
	}
}

static void portrait_vline(const struct display_capabilities *caps,
			   int x, int y0, int y1)
{
	if (y0 > y1) {
		int tmp = y0;
		y0 = y1;
		y1 = tmp;
	}

	for (int y = y0; y <= y1; ++y) {
		portrait_set_pixel(caps, x, y, true);
	}
}

static void portrait_rect(const struct display_capabilities *caps,
			  int x, int y, int width, int height)
{
	portrait_hline(caps, x, x + width - 1, y);
	portrait_hline(caps, x, x + width - 1, y + height - 1);
	portrait_vline(caps, x, y, y + height - 1);
	portrait_vline(caps, x + width - 1, y, y + height - 1);
}

static void portrait_diag(const struct display_capabilities *caps,
			  int x0, int y0, int x1, int y1)
{
	int dx = (x1 >= x0) ? 1 : -1;
	int dy = (y1 >= y0) ? 1 : -1;
	int length = (x1 >= x0) ? x1 - x0 : x0 - x1;

	for (int i = 0; i <= length; ++i) {
		portrait_set_pixel(caps, x0 + i * dx, y0 + i * dy, true);
	}
}

static void oled_portrait_build_calibration(
	const struct display_capabilities *caps)
{
	memset(oled_portrait_framebuffer, 0xff,
	       sizeof(oled_portrait_framebuffer));

	/* Left edge ladder: x=0..3, with distinct stepped lengths. */
	portrait_vline(caps, 0, 10, 117);
	portrait_vline(caps, 1, 14, 113);
	portrait_vline(caps, 2, 18, 109);
	portrait_vline(caps, 3, 22, 105);

	/* Right edge ladder: x=60..63, mirrored stepped lengths. */
	portrait_vline(caps, 60, 22, 105);
	portrait_vline(caps, 61, 18, 109);
	portrait_vline(caps, 62, 14, 113);
	portrait_vline(caps, 63, 10, 117);

	/* Short horizontal line exactly at portrait y=64. */
	portrait_hline(caps, 22, 42, 64);

	/* Upper-center arrow pointing toward portrait y=0. */
	portrait_vline(caps, 32, 21, 50);
	portrait_diag(caps, 24, 28, 32, 20);
	portrait_diag(caps, 40, 28, 32, 20);
}

static int oled_portrait_flush(const struct device *display)
{
	int ret = display_write(display, 0, 0,
			       &oled_portrait_buffer_descriptor,
			       oled_portrait_framebuffer);

	if (ret < 0) {
		return ret;
	}

	return display_blanking_off(display);
}

static void oled_portrait_work_handler(struct k_work *work)
{
	struct display_capabilities caps;
	int ret;

	ARG_UNUSED(work);

	LOG_INF("build marker: %s", oled_portrait_build_marker);

	if (!device_is_ready(oled_portrait_display)) {
		LOG_ERR("display device %s is not ready; portrait calibration aborted",
			oled_portrait_display->name);
		return;
	}

	display_get_capabilities(oled_portrait_display, &caps);
	LOG_INF("capabilities x=%u y=%u supported=0x%08x current=0x%x "
		"screen_info=0x%08x VTILED=%s MSB_FIRST=%s",
		caps.x_resolution, caps.y_resolution,
		caps.supported_pixel_formats, caps.current_pixel_format,
		caps.screen_info,
		(caps.screen_info & SCREEN_INFO_MONO_VTILED) ? "yes" : "no",
		(caps.screen_info & SCREEN_INFO_MONO_MSB_FIRST) ? "yes" : "no");

	if (caps.x_resolution != PHYSICAL_WIDTH ||
	    caps.y_resolution != PHYSICAL_HEIGHT ||
	    !(caps.supported_pixel_formats & PIXEL_FORMAT_MONO10) ||
	    caps.current_pixel_format != PIXEL_FORMAT_MONO10 ||
	    !(caps.screen_info & SCREEN_INFO_MONO_VTILED)) {
		LOG_ERR("unsupported SSD1306 capabilities; portrait calibration aborted");
		return;
	}

	ret = oled_portrait_clear(oled_portrait_display);
	if (ret < 0) {
		LOG_ERR("oled_portrait_clear failed: %d", ret);
		return;
	}

	oled_portrait_build_calibration(&caps);
	ret = oled_portrait_flush(oled_portrait_display);
	if (ret < 0) {
		LOG_ERR("oled_portrait_flush failed: %d", ret);
		return;
	}

	LOG_INF("RAW portrait geometry calibration written once");
}

static K_WORK_DELAYABLE_DEFINE(oled_portrait_work,
			       oled_portrait_work_handler);

static int oled_portrait_schedule(void)
{
	int ret = k_work_schedule(&oled_portrait_work, K_SECONDS(2));

	if (ret < 0) {
		LOG_ERR("could not schedule RAW portrait calibration: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(oled_portrait_schedule, APPLICATION, 0);
