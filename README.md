
# Build & Flash

## Prepare

```
cd ~/zephyrproject/zephyr
source ~/zephyrproject/.venv/bin/activate
```

ZEPHYR_BASE=~/zephyrproject/zephyr

## Build

```
west build -p always -b esp32_devkitc/esp32/procpu ~/project_zephyr/lcd_app
```

```
west build -p always -b esp32_devkitc/esp32/procpu \
  ~/zephyrproject/zephyr/samples/drivers/display \
  -DDTC_OVERLAY_FILE=~/project_zephyr/lcd_app/boards/esp32_devkitc_esp32_procpu.overlay
```

```
west build -p always -b esp32_devkitc/esp32/procpu \
  ~/zephyrproject/zephyr/samples/subsys/display/lvgl \
  -DDTC_OVERLAY_FILE=~/project_zephyr/lcd_app/boards/esp32_devkitc_esp32_procpu.overlay
```

## Flash

```
west flash
```

## Monitor

```
west espressif monitor
```
