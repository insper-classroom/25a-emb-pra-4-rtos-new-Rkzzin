/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;
const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;
const uint TRIG_PIN = 17;
const uint ECHO_PIN = 16;

volatile uint64_t time_init = 0;
volatile uint64_t time_end = 0;

void pin_callback(uint gpio, uint32_t events) {
    if (events == 0x4) {
        time_end = to_us_since_boot(get_absolute_time());
    } else if (events == 0x8) {
        time_init = to_us_since_boot(get_absolute_time());
    }
}

void trigger_task (void *p) {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    while (1) {
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echo_task (void *p) {
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

float get_distance() {
    int duration = time_end - time_init;
    return (duration * 0.034) / 2.0;
}

void oled_task (void *p) {
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    while (1) {
        gfx_clear_buffer(&disp);
        gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
        gfx_draw_string(&disp, 0, 10, 1, "cm");
        float dist = get_distance();
        char dist_str[10];
        sprintf(dist_str, "%.2f", dist);
        gfx_draw_string(&disp, 0, 20, 1, dist_str);
        gfx_show(&disp);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int main() {
    stdio_init_all();

    
    xTaskCreate(trigger_task, "Trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
