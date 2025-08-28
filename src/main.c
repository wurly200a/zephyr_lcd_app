// src/main.c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* あなたの配線：バックライト = IO32（ACTIVE_HIGH前提） */
#define BLK_GPIO_NODE  DT_NODELABEL(gpio0)
#define BLK_PIN        32

static const struct device *gpio0_dev;

/* 画面にテキストを出す（CFB使用） */
static int draw_text_cfb(const struct device *disp)
{
	int rc;

	/* CFB 初期化 */
	rc = cfb_framebuffer_init(disp);
	if (rc) {
		LOG_ERR("cfb_framebuffer_init failed: %d", rc);
		return rc;
	}

	/* 使えるフォント一覧をログに吐きつつ、0番フォントを選択 */
	int fonts = cfb_get_numof_fonts(disp);
	for (int i = 0; i < fonts; i++) {
		uint8_t fw, fh;
		cfb_get_font_size(disp, i, &fw, &fh);
		LOG_INF("font[%d] = %ux%u", i, fw, fh);
	}
	cfb_framebuffer_set_font(disp, 0);

	/* 画面サイズ（display capabilities から取得） */
	struct display_capabilities cap;
	display_get_capabilities(disp, &cap);
	uint16_t W = cap.x_resolution;
	uint16_t H = cap.y_resolution;
	LOG_INF("cap: %ux%u", W, H);

	/* 一旦クリアして “Hello/Counter” を表示 */
	cfb_framebuffer_clear(disp, true);

	const char *title = "Zephyr CFB demo";
	int x = 4, y = 4;
	cfb_print(disp, title, x, y);

	/* 下の方にカウンタを表示して更新していく */
	int cx = 4, cy = H / 2;
	char buf[32];

	/* 最初のコミット（描画反映） */
	cfb_framebuffer_finalize(disp);

	/* カウンタを定期更新 */
	for (int sec = 0;; sec++) {
		snprintf(buf, sizeof(buf), "sec=%d", sec);

		/* カウンタ行だけ上書きしたいので、簡便に全クリア→再描画 */
		cfb_framebuffer_clear(disp, false);
		cfb_print(disp, title, x, y);
		cfb_print(disp, buf,   cx, cy);

		/* 反映 */
		cfb_framebuffer_finalize(disp);
		k_msleep(1000);
	}

	/* ここには来ない */
	// return 0;
}

void main(void)
{
	/* --- バックライト ON（IO32, ACTIVE_HIGH 想定） --- */
	gpio0_dev = DEVICE_DT_GET(BLK_GPIO_NODE);
	if (!device_is_ready(gpio0_dev)) {
		LOG_ERR("gpio0 not ready");
		return;
	}
	/* 初期値=High（点灯）。もし逆極性なら GPIO_OUTPUT_INACTIVE に */
	int rc = gpio_pin_configure(gpio0_dev, BLK_PIN, GPIO_OUTPUT_ACTIVE);
	if (rc) {
		LOG_ERR("gpio_pin_configure(BL) failed: %d", rc);
		return;
	}
	/* 少し待ってから描画開始 */
	k_msleep(10);

	/* --- Display 取得・初期化 --- */
	const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(disp)) {
		LOG_ERR("Display device not ready");
		return;
	}

	/* ILI9341 は RGB565 が標準 */
	(void)display_set_pixel_format(disp, PIXEL_FORMAT_RGB_565);

	/* ブランキング解除（消灯状態解除） */
	(void)display_blanking_off(disp);

	LOG_INF("Display ready, start CFB");
	(void)draw_text_cfb(disp);
}
