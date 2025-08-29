
# Build & Flash

## Prepare

```
cd ~/zephyrproject/zephyr
source ~/zephyrproject/.venv/bin/activate
```

if necessary

```
ZEPHYR_BASE=~/zephyrproject/zephyr
```

## Build Sample

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

## Build

```
west build -p always -b esp32_devkitc/esp32/procpu -S psram-4M -S psram-wifi ~/project_zephyr/lcd_app
```

## Flash

```
west flash
```

## Monitor

```
west espressif monitor
```
