#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* BL=IO32（HIGHで点灯。基板が逆なら GPIO_OUTPUT_INACTIVE に） */
#define BLK_PORT DT_NODELABEL(gpio0)
#define BLK_PIN  32

/* 1秒ごとにラベルを更新（v9はアクセサでuser_data取得） */
static void timer_cb(lv_timer_t *timer)
{
    lv_obj_t *label = (lv_obj_t *)lv_timer_get_user_data(timer);
    static int sec;
    lv_label_set_text_fmt(label, "sec=%d", sec++);
}

void main(void)
{
    /* バックライト ON */
    const struct device *gpio0 = DEVICE_DT_GET(BLK_PORT);
    if (device_is_ready(gpio0)) {
        gpio_pin_configure(gpio0, BLK_PIN, GPIO_OUTPUT_ACTIVE);
    }

    /* ディスプレイ */
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp)) {
        LOG_ERR("display not ready");
        return;
    }
    display_blanking_off(disp);

    struct display_capabilities cap;
    display_get_capabilities(disp, &cap);
    LOG_INF("Display %ux%u rotation ok", cap.x_resolution, cap.y_resolution);

    /* 背景グレー＋文字は白 */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    static lv_style_t st_white;
    lv_style_init(&st_white);
    lv_style_set_text_color(&st_white, lv_color_white());
    lv_style_set_text_font(&st_white, LV_FONT_DEFAULT);  /* Kconfigの既定フォント */
    lv_obj_add_style(scr, &st_white, 0);

    /* タイトル */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "LVGL on Zephyr (ILI9341)");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 8);

    /* サブテキスト */
    lv_obj_t *hello = lv_label_create(scr);
    lv_label_set_text(hello, "Hello, world! 1234");
    lv_obj_align(hello, LV_ALIGN_TOP_LEFT, 8, 30);

    /* カウンタ */
    lv_obj_t *cnt = lv_label_create(scr);
    lv_label_set_text(cnt, "sec=0");
    lv_obj_align(cnt, LV_ALIGN_TOP_LEFT, 8, 50);
    (void)lv_timer_create(timer_cb, 1000, cnt);

    /* 赤いテストボックス（画素描画の確認） */
    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_set_size(box, 120, 40);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFF0000), 0);
    lv_obj_align(box, LV_ALIGN_BOTTOM_RIGHT, -8, -8);

    /* 初回強制リフレッシュ */
    lv_refr_now(NULL);

    for (;;) {
        k_sleep(K_SECONDS(60));
    }
}
