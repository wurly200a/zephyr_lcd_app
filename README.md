
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

## Flash

```
west flash
```

## Monitor

```
west espressif monitor
```
