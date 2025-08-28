#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* BL=IO32 を点灯（ACTIVE_HIGH 想定） */
#define BL_PORT DT_NODELABEL(gpio0)
#define BL_PIN  32

#define MAX_W   320
#define TILE_H  24
static uint16_t linebuf[MAX_W * TILE_H];

static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	return sys_cpu_to_be16(v); /* ILI9341 は上位バイト先送 */
}

static void fill_rect(const struct device *disp,
		      uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		      uint16_t color_be)
{
	struct display_buffer_descriptor desc = {
		.width = w, .height = 0, .pitch = w, .buf_size = 0
	};
	for (uint16_t yy = 0; yy < h; yy += TILE_H) {
		uint16_t hh = (h - yy) < TILE_H ? (h - yy) : TILE_H;
		size_t px = (size_t)w * hh;
		for (size_t i = 0; i < px; i++) linebuf[i] = color_be;
		desc.height = hh;
		desc.buf_size = (uint32_t)w * hh * 2U;
		int rc = display_write(disp, x, y + yy, &desc, linebuf);
		if (rc) { LOG_ERR("display_write rc=%d", rc); return; }
	}
}

void main(void)
{
	/* BL ON */
	const struct device *g0 = DEVICE_DT_GET(BL_PORT);
	if (device_is_ready(g0)) gpio_pin_configure(g0, BL_PIN, GPIO_OUTPUT_ACTIVE);

	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(disp)) { LOG_ERR("display NG"); return; }

	(void)display_set_pixel_format(disp, PIXEL_FORMAT_RGB_565);
	(void)display_blanking_off(disp);

	struct display_capabilities cap;
	display_get_capabilities(disp, &cap);
	uint16_t W = cap.x_resolution, H = cap.y_resolution;
	LOG_INF("Display %ux%u", W, H);

	/* まずは単色でノイズの有無を見る */
	for (;;) {
		fill_rect(disp, 0, 0, W, H, rgb565_be(0,0,0));   k_msleep(800); /* 黒 */
		fill_rect(disp, 0, 0, W, H, rgb565_be(255,0,0)); k_msleep(800); /* 赤 */
		fill_rect(disp, 0, 0, W, H, rgb565_be(0,255,0)); k_msleep(800); /* 緑 */
		fill_rect(disp, 0, 0, W, H, rgb565_be(0,0,255)); k_msleep(800); /* 青 */
	}
}
