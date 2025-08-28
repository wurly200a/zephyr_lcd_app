#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* あなたの配線: BL=IO32（HIGHで点灯。逆なら OUTPUT_INACTIVE に） */
#define BLK_PORT DT_NODELABEL(gpio0)
#define BLK_PIN  32

/* ---- LVGLスレッド側で実行するコールバック ---- */
static void ui_build_cb(void *user_data);
static void tick_cb(lv_timer_t *t);

/* UI要素（タイマから参照） */
static lv_obj_t *label_counter;

void main(void)
{
    /* バックライト ON */
    const struct device *gpio0 = DEVICE_DT_GET(BLK_PORT);
    if (device_is_ready(gpio0)) {
        gpio_pin_configure(gpio0, BLK_PIN, GPIO_OUTPUT_ACTIVE);
    }

    /* ディスプレイ → ブランキング解除（LVGLとは独立） */
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp)) {
        LOG_ERR("display not ready");
        return;
    }
    (void)display_blanking_off(disp);

    /* LVGLのデフォルト表示が用意されるまで待つ */
    while (lv_disp_get_default() == NULL) {
        k_msleep(10);
    }
    LOG_INF("LVGL ready");

    /* ★ UI作成は LVGL スレッドで実行 */
    lv_async_call(ui_build_cb, NULL);

    /* 以後は待機（描画はLVGLスレッドが回す） */
    for (;;) {
        k_sleep(K_FOREVER);
    }
}

/* 追加: 背景色を順に切り替える */
static void bg_cycle_cb(lv_timer_t *t) {
    static int i = 0;
    lv_color_t colors[] = {
        lv_color_white(), lv_color_black(),
        lv_color_make(255,0,0),   // red
        lv_color_make(0,255,0),   // green
        lv_color_make(0,0,255),   // blue
        lv_color_make(255,255,0), // yellow
        lv_color_make(0,255,255), // cyan
        lv_color_make(255,0,255), // magenta
    };
    lv_obj_set_style_bg_color(lv_scr_act(), colors[i], 0);
    i = (i + 1) % (int)(sizeof(colors)/sizeof(colors[0]));
}

/* ---- ここは LVGL スレッド内で実行される ---- */
static void ui_build_cb(void *user_data)
{
    LV_UNUSED(user_data);

    /* 背景白、中央に “Hello LVGL” */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);

    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_center(label);

    /* 右下にカウンタ、1秒毎に更新 */
    label_counter = lv_label_create(lv_scr_act());
    lv_obj_align(label_counter, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_label_set_text(label_counter, "sec=0");

    lv_timer_create(tick_cb, 1000, label_counter);
    lv_timer_create(bg_cycle_cb, 800, NULL);
}

static void tick_cb(lv_timer_t *t)
{
    static int sec = 0;
    lv_obj_t *lbl = (lv_obj_t *)lv_timer_get_user_data(t);
    if (lbl) {
        lv_label_set_text_fmt(lbl, "sec=%d", sec++);
    }
}
