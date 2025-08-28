// src/main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* ILI9341 想定（最大 320x240）。行分割して転送 */
#define MAX_WIDTH  320
#define TILE_H       24
static uint16_t linebuf[MAX_WIDTH * TILE_H];

/* 0–255 の RGB を RGB565（ビッグエンディアン）に変換 */
static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
	return sys_cpu_to_be16(v);
}

/* 矩形塗りつぶし（分割書き込み） */
static void blit_rect(const struct device *disp,
		      uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		      uint16_t color_be)
{
	struct display_buffer_descriptor desc = {
		.width  = w,
		.height = 0,      /* ループで設定 */
		.pitch  = w,      /* ピクセル数 */
		.buf_size = 0     /* バイト数。ループで設定 */
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

/* カラーバー描画（動作確認用） */
static void draw_color_bars(const struct device *disp,
			    uint16_t w, uint16_t h)
{
	const uint16_t colors[] = {
		rgb565_be(255,   0,   0), /* Red    */
		rgb565_be(  0, 255,   0), /* Green  */
		rgb565_be(  0,   0, 255), /* Blue   */
		rgb565_be(255, 255, 255), /* White  */
		rgb565_be(255, 255,   0), /* Yellow */
		rgb565_be(  0, 255, 255), /* Cyan   */
		rgb565_be(255,   0, 255), /* Magenta*/
		rgb565_be(  0,   0,   0), /* Black  */
	};
	const size_t n = ARRAY_SIZE(colors);

	uint16_t bar_w = w / n;
	for (size_t i = 0; i < n; i++) {
		uint16_t x  = (uint16_t)(i * bar_w);
		uint16_t ww = (i == n - 1) ? (w - x) : bar_w;
		blit_rect(disp, x, 0, ww, h, colors[i]);
	}
}

/* 画面全体フェード（任意） */
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
	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(disp)) {
		LOG_ERR("Display device not ready");
		return;
	}

	/* ピクセルフォーマットは API で指定（構造体ではない） */
	(void)display_set_pixel_format(disp, PIXEL_FORMAT_RGB_565);

	/* 初期状態でブランキングされている場合があるので解除 */
	(void)display_blanking_off(disp);

	struct display_capabilities cap;
	display_get_capabilities(disp, &cap);

	uint16_t W = cap.x_resolution;
	uint16_t H = cap.y_resolution;
	LOG_INF("Display ready: %ux%u", W, H);

	while (1) {
		draw_color_bars(disp, W, H);
		k_msleep(1000);

		fade_solid(disp, W, H, 255, 255, 255); /* 白へフェード */
		k_msleep(300);

		fade_solid(disp, W, H, 0, 0, 0);       /* 黒へフェード */
		k_msleep(300);
	}
}
