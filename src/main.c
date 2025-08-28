// src/main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* ILI9341 想定。行分割して転送 */
#define MAX_WIDTH  320
#define TILE_H       24
static uint16_t linebuf[MAX_WIDTH * TILE_H];

/* あなたの配線: バックライト LED = IO32 */
#define BLK_GPIO_PORT_NODE DT_NODELABEL(gpio0)
#define BLK_PIN 32
static const struct device *gpio0_dev;

static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	return sys_cpu_to_be16(v);
}

static void blit_rect(const struct device *disp,
		      uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		      uint16_t color_be)
{
	struct display_buffer_descriptor desc = {
		.width  = w,
		.height = 0,
		.pitch  = w,
		.buf_size = 0
	};

	for (uint16_t yy = 0; yy < h; yy += TILE_H) {
		uint16_t chunk_h = MIN((uint16_t)TILE_H, (uint16_t)(h - yy));
		size_t px = (size_t)w * chunk_h;

		for (size_t i = 0; i < px; i++) {
			linebuf[i] = color_be;
		}

		desc.height  = chunk_h;
		desc.buf_size = (uint32_t)w * chunk_h * 2U;

		int rc = display_write(disp, x, y + yy, &desc, linebuf);
		if (rc) {
			LOG_ERR("display_write failed: %d (x=%u y=%u w=%u h=%u)",
				rc, x, (uint16_t)(y + yy), w, chunk_h);
			return;
		}
	}
}

static void draw_color_bars(const struct device *disp, uint16_t w, uint16_t h)
{
	const uint16_t colors[] = {
		rgb565_be(255,   0,   0),
		rgb565_be(  0, 255,   0),
		rgb565_be(  0,   0, 255),
		rgb565_be(255, 255, 255),
		rgb565_be(255, 255,   0),
		rgb565_be(  0, 255, 255),
		rgb565_be(255,   0, 255),
		rgb565_be(  0,   0,   0),
	};
	const size_t n = ARRAY_SIZE(colors);

	uint16_t bar_w = w / n;
	for (size_t i = 0; i < n; i++) {
		uint16_t x  = (uint16_t)(i * bar_w);
		uint16_t ww = (i == n - 1) ? (w - x) : bar_w;
		blit_rect(disp, x, 0, ww, h, colors[i]);
	}
}

static void fade_solid(const struct device *disp, uint16_t w, uint16_t h,
		       uint8_t r, uint8_t g, uint8_t b)
{
	for (int step = 0; step <= 255; step += 32) {
		uint16_t c = rgb565_be((uint8_t)((r * step) / 255),
				       (uint8_t)((g * step) / 255),
				       (uint8_t)((b * step) / 255));
		blit_rect(disp, 0, 0, w, h, c);
		k_msleep(40);
	}
}

void main(void)
{
	/* ★ バックライト(IO2)をON：起動直後に点ける */
	gpio0_dev = DEVICE_DT_GET(BLK_GPIO_PORT_NODE);
	if (!device_is_ready(gpio0_dev)) {
		LOG_ERR("gpio0 not ready");
		return;
	}
	/* 出力Highで点灯（必要なら ACTIVE_LOW に変更） */
	gpio_pin_configure(gpio0_dev, BLK_PIN, GPIO_OUTPUT_ACTIVE);

	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(disp)) {
		LOG_ERR("Display device not ready");
		return;
	}

	int rc = display_set_pixel_format(disp, PIXEL_FORMAT_RGB_565);
	if (rc) {
		LOG_WRN("display_set_pixel_format RGB565 rc=%d", rc);
	}

	(void)display_blanking_off(disp);

	struct display_capabilities cap;
	display_get_capabilities(disp, &cap);
	LOG_INF("Display ready: %ux%u", cap.x_resolution, cap.y_resolution);

	uint16_t W = cap.x_resolution;
	uint16_t H = cap.y_resolution;

	while (1) {
		draw_color_bars(disp, W, H);
		k_msleep(1000);
		fade_solid(disp, W, H, 255, 255, 255);
		k_msleep(300);
		fade_solid(disp, W, H, 0, 0, 0);
		k_msleep(300);
	}
}
