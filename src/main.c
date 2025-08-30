#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <inttypes.h>
#include <lvgl.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* === Backlight: IO32（HIGHで点灯。基板が逆なら GPIO_OUTPUT_INACTIVE に） === */
#define BLK_PORT DT_NODELABEL(gpio0)
#define BLK_PIN  32

#define CLAMPI(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

/* === RAW→UI: inputコールバックでmsgqに積み、mainループでUI更新 === */
struct touch_evt { int16_t x, y; uint8_t pressed; };
K_MSGQ_DEFINE(touch_msgq, sizeof(struct touch_evt), 16, 4);

/* 画面部品 */
static lv_obj_t *lbl_title, *lbl_xy, *lbl_hb, *cursor;
static int16_t scr_w, scr_h;

/* ---- 座標変換モード（ビット） ----
 * bit0: SWAP_XY
 * bit1: INVERT_X
 * bit2: INVERT_Y
 * 例) 5(=0b101) → SWAP + INVERT_Y
 */
enum { MAP_SWAP=0x1, MAP_INVX=0x2, MAP_INVY=0x4 };
static uint8_t g_map = (MAP_SWAP | MAP_INVX | MAP_INVY);  /* ← rotation=90 の定番初期値 */

static inline void map_touch_to_screen(int rx, int ry, int *sx, int *sy)
{
    int x = rx, y = ry;
    if (g_map & MAP_SWAP) { int t = x; x = y; y = t; }
    if (g_map & MAP_INVX) x = (scr_w - 1) - x;
    if (g_map & MAP_INVY) y = (scr_h - 1) - y;
    *sx = CLAMPI(x, 0, scr_w - 1);
    *sy = CLAMPI(y, 0, scr_h - 1);
}

static void show_map_on_title(void)
{
    lv_label_set_text_fmt(lbl_title,
        "LVGL on Zephyr (ILI9341) — map=%u [%s%s%s]",
        g_map,
        (g_map&MAP_SWAP) ? "SWAP " : "",
        (g_map&MAP_INVX) ? "INVX " : "",
        (g_map&MAP_INVY) ? "INVY " : "");
}

/* RAWのダンプ＋最新だけmsgqへ投入 */
static void raw_input_cb(struct input_event *evt, void *user_data)
{
    static int32_t rx, ry; static int pressed;
    if (evt->type == INPUT_EV_ABS) {
        if (evt->code == INPUT_ABS_X) rx = evt->value;
        if (evt->code == INPUT_ABS_Y) ry = evt->value;
    } else if (evt->type == INPUT_EV_KEY && evt->code == INPUT_BTN_TOUCH) {
        pressed = (evt->value != 0);
    }
    if (evt->sync) {
        LOG_INF("[RAW] dev=%s %s x=%" PRId32 " y=%" PRId32,
                evt->dev ? evt->dev->name : "?", pressed ? "DOWN":"UP", rx, ry);
        struct touch_evt m = { .x=(int16_t)rx, .y=(int16_t)ry, .pressed=(uint8_t)pressed };
        if (k_msgq_put(&touch_msgq, &m, K_NO_WAIT) != 0) {
            struct touch_evt drop; (void)k_msgq_get(&touch_msgq, &drop, K_NO_WAIT);
            (void)k_msgq_put(&touch_msgq, &m, K_NO_WAIT);
        }
    }
}
/* xpt2046ノードだけ監視（全デバイスを見るなら第1引数をNULL） */
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(xpt2046)), raw_input_cb, NULL);

/* --- Shell: touch_map <0..7> / touch_map show --- */
static int cmd_touch_map(const struct shell *shell, size_t argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "show") == 0) {
        shell_print(shell, "map=%u (bits: swap=%d invx=%d invy=%d)",
            g_map, !!(g_map&MAP_SWAP), !!(g_map&MAP_INVX), !!(g_map&MAP_INVY));
        return 0;
    }
    if (argc != 2) {
        shell_error(shell, "usage: touch_map <0..7>|show");
        return -EINVAL;
    }
    char *endp = NULL;
    long v = strtol(argv[1], &endp, 0);
    if (endp==argv[1] || v < 0 || v > 7) {
        shell_error(shell, "invalid value: %s (0..7)", argv[1]);
        return -EINVAL;
    }
    g_map = (uint8_t)v;
    LOG_INF("touch_map set to %u", g_map);
    if (lbl_title) show_map_on_title(); /* 画面にも反映 */
    return 0;
}
SHELL_CMD_ARG_REGISTER(touch_map, NULL, "set touch mapping 0..7 or 'show'", cmd_touch_map, 2, 0);

void main(void)
{
    /* --- バックライト ON --- */
    const struct device *gpio0 = DEVICE_DT_GET(BLK_PORT);
    if (device_is_ready(gpio0)) {
        gpio_pin_configure(gpio0, BLK_PIN, GPIO_OUTPUT_ACTIVE);
    }

    /* --- ディスプレイ --- */
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(disp)) { LOG_ERR("display not ready"); return; }
    display_blanking_off(disp);

    struct display_capabilities cap;
    display_get_capabilities(disp, &cap);
    scr_w = (int16_t)cap.x_resolution;
    scr_h = (int16_t)cap.y_resolution;
    LOG_INF("Display %ux%u rotation ok", cap.x_resolution, cap.y_resolution);

    /* --- 背景＆基本スタイル --- */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    static lv_style_t st_white; lv_style_init(&st_white);
    lv_style_set_text_color(&st_white, lv_color_white());
    lv_style_set_text_font(&st_white, LV_FONT_DEFAULT);
    lv_obj_add_style(scr, &st_white, 0);

    /* --- タイトル（現在のmapを表示） --- */
    lbl_title = lv_label_create(scr);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 8, 8);
    show_map_on_title();

    /* --- 心拍表示 --- */
    lbl_hb = lv_label_create(scr);
    lv_label_set_text(lbl_hb, "hb=0");
    lv_obj_align(lbl_hb, LV_ALIGN_TOP_RIGHT, -8, 8);

    /* --- 座標表示 --- */
    lbl_xy = lv_label_create(scr);
    lv_label_set_text(lbl_xy, "Touch: (waiting)");
    lv_obj_align(lbl_xy, LV_ALIGN_TOP_LEFT, 8, 30);
#if defined(CONFIG_LV_FONT_MONTSERRAT_20)
    extern const lv_font_t lv_font_montserrat_20;
    lv_obj_set_style_text_font(lbl_xy, &lv_font_montserrat_20, 0);
#endif

    /* --- 白丸カーソル --- */
    cursor = lv_obj_create(scr);
    lv_obj_set_size(cursor, 12, 12);
    lv_obj_set_style_bg_color(cursor, lv_color_white(), 0);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(cursor, 0, 0);
    lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);

    /* 初回表示 */
    lv_refr_now(NULL);

    /* --- メイン“手動ポンプ”ループ --- */
    uint32_t hb = 0; int64_t next_hb = k_uptime_get() + 500;
    for (;;) {
        struct touch_evt m, last; bool have = false;
        /* 10msで掃いて最後の1件採用 */
        for (int i=0;i<10;i++) {
            if (k_msgq_get(&touch_msgq, &m, K_MSEC(1)) == 0) { last = m; have = true; }
        }
        if (have) {
            int sx, sy;
            map_touch_to_screen(last.x, last.y, &sx, &sy);
            lv_label_set_text_fmt(lbl_xy, "Touch: %s  x=%d  y=%d",
                                  last.pressed ? "DOWN" : "UP", sx, sy);
            if (last.pressed) {
                lv_obj_clear_flag(cursor, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(cursor, sx - 6, sy - 6);
            } else {
                lv_obj_add_flag(cursor, LV_OBJ_FLAG_HIDDEN);
            }
            lv_refr_now(NULL);
        }
        int64_t now = k_uptime_get();
        if (now >= next_hb) {
            lv_label_set_text_fmt(lbl_hb, "hb=%u", ++hb);
            lv_refr_now(NULL);
            next_hb = now + 500;
        }
        k_msleep(2);
    }
}
