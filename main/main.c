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

const uint TRIG_PIN = 17;
const uint ECHO_PIN = 16;

QueueHandle_t xQueueTime;               // Fila com informação do tempo to_us_since_boot
QueueHandle_t xQueueDistance;           // Valor da distância em cm lido pela task_echo
SemaphoreHandle_t xSemaphoreTrigger;    // Avisa o OLED que uma leitura foi disparada


void pin_callback(uint gpio, uint32_t events) {
    uint64_t time;
    if (events == 0x4) {
        time = to_us_since_boot(get_absolute_time());
    } else if (events == 0x8) {
        time = to_us_since_boot(get_absolute_time());
    }

    xQueueSendFromISR(xQueueTime, &time, 0);
}


void trigger_task (void *p) {
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);

    while (1) {
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echo_task (void *p) {
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    uint64_t time_init = 0;
    uint64_t time_end;

    while (1) {
        if (xQueueReceive(xQueueTime, &time_init, pdMS_TO_TICKS(100))) {
            xQueueReceive(xQueueTime, &time_end, pdMS_TO_TICKS(100));

            float duration = (float)(time_end - time_init);
            float distance = (duration * 0.034) / 2;

            printf("Distancia: %.2f cm\n", distance);

            xQueueSend(xQueueDistance, &distance, pdMS_TO_TICKS(100));
        }
    }
}

void oled_task (void *p) {
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    float dist;
    char dist_str[10];

    while (1) {
        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (xQueueReceive(xQueueDistance, &dist, pdMS_TO_TICKS(100))) {
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                gfx_draw_string(&disp, 0, 10, 1, "cm");
                
                if (dist > 300) {
                    gfx_draw_string(&disp, 0, 20, 1, "Fora de alcance");
                } else {
                    sprintf(dist_str, "%.2f", dist);
                    gfx_draw_string(&disp, 0, 20, 1, dist_str);
                }
                
                gfx_show(&disp);
            } else {
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Fio Desconectado");
                gfx_show(&disp);
            }
        } 
    }
}

int main() {
    stdio_init_all();

    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(32, sizeof(uint64_t));
    xQueueDistance = xQueueCreate(32, sizeof(float));

    xTaskCreate(trigger_task, "Trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true) {}
}
