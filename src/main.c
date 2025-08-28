#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <string.h>

void main(void)
{
	while (1) {
		k_msleep(1000);
	}
}
