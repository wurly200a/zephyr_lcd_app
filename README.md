
# Build & Flash & Monitor

```
cd ~/zephyrproject/zephyr
source ~/zephyrproject/.venv/bin/activate
ZEPHYR_BASE=~/zephyrproject/zephyr
west build -p always -b esp32_devkitc/esp32/procpu -S psram-4M -S psram-wifi ~/project/zephyr_lcd_app
west flash
west espressif monitor
```

# Build & Flash & Monitor (using docker container)

```
cd ~/project/zephyr_lcd_app
DEV=/dev/ttyUSB0;docker run --rm -it --device=${DEV} --group-add $(stat -c '%g' ${DEV}) -v ${PWD}:/home/builder/work -w /home/builder/zephyrproject ghcr.io/wurly200a/builder-zephyr-esp32/zephyr-esp32:latest
west build -p always -b esp32_devkitc/esp32/procpu -S psram-4M -S psram-wifi ~/work
west flash
west espressif monitor
```
