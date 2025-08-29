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

/* === 追加: タッチ可視化 === */
static void touch_ev_cb(lv_event_t *e)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    lv_point_t p = { -1, -1 };
    lv_indev_t *indev = lv_indev_get_act(); /* 現在処理中の入力デバイス（ポインタ想定） */
    if (indev && lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
        lv_indev_get_point(indev, &p);
    }

    if (code == LV_EVENT_PRESSED) {
        lv_label_set_text_fmt(label, "Touch: PRESS  x=%d y=%d", p.x, p.y);
    } else if (code == LV_EVENT_PRESSING) {
        lv_label_set_text_fmt(label, "Touch: DRAG   x=%d y=%d", p.x, p.y);
    } else if (code == LV_EVENT_RELEASED) {
        lv_label_set_text_fmt(label, "Touch: RELEASE x=%d y=%d", p.x, p.y);
    }
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

    /* 1) タッチ状態を表示するラベル */
    lv_obj_t *touch_lbl = lv_label_create(scr);
    lv_label_set_text(touch_lbl, "Touch: (waiting)");
    lv_obj_align(touch_lbl, LV_ALIGN_BOTTOM_LEFT, 8, -56);

    /* ルート画面をクリック可能にしてイベントを受ける */
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, touch_ev_cb, LV_EVENT_PRESSED,   touch_lbl);
    lv_obj_add_event_cb(scr, touch_ev_cb, LV_EVENT_PRESSING,  touch_lbl);
    lv_obj_add_event_cb(scr, touch_ev_cb, LV_EVENT_RELEASED,  touch_lbl);

    /* 2) ポインタにカーソル丸を付ける（指の位置が可視化される） */
    lv_indev_t *indev = NULL;
    for (indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_obj_t *cursor = lv_obj_create(scr);
            lv_obj_set_size(cursor, 12, 12);
            lv_obj_set_style_bg_color(cursor, lv_color_white(), 0);
            lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(cursor, 0, 0);
            lv_indev_set_cursor(indev, cursor);
            break;
        }
    }

    /* 初回強制リフレッシュ */
    lv_refr_now(NULL);

    for (;;) {
        k_sleep(K_SECONDS(60));
    }
}
