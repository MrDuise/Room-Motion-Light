#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_err.h"



#define UART_NUM UART_NUM_2
#define BUF_SIZE (1024)
#define TXD_PIN (17)
#define RXD_PIN (16)


// A unique tag identifier for our debug logs
static const char *TAG = "MAIN_APP";
//mostly likely needs a different name than the one in main.c, but for now, let's keep it simple and use the same name.
// static const char *TAG = "LD2410";

QueueHandle_t dummy_queue;
QueueHandle_t uart_queue;




void ld2410cSetup(void) {
    const int uart_buffer_size = (BUF_SIZE * 2);
    

    uart_config_t uart_config = {
        .baud_rate = 256000,              
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, 
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}



// Dedicated asynchronous event receiver task
void ld2410cEventTask(void *pvParameter) {
    uart_event_t event;
    uint8_t *dtbuf = (uint8_t *)malloc(BUF_SIZE);
    uint8_t data[BUF_SIZE];
    uint8_t frame_buf[23];

    for (;;) {
       // Read incoming bytes from the sensor (timeout after 20ms)
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, pdMS_TO_TICKS(20));
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                // Shift frame buffer left by 1 byte and insert the newest byte
                for (int j = 0; j < 22; j++) {
                    frame_buf[j] = frame_buf[j + 1];
                }
                frame_buf[22] = data[i];

                // Check for Header (F4 F3 F2 F1) and Footer (F8 F7 F6 F5)
                if (frame_buf[0] == 0xF4 && frame_buf[1] == 0xF3 && 
                    frame_buf[2] == 0xF2 && frame_buf[3] == 0xF1 &&
                    frame_buf[19] == 0xF8 && frame_buf[20] == 0xF7 && 
                    frame_buf[21] == 0xF6 && frame_buf[22] == 0xF5) {
                    
                    // Verify data type is Basic Reporting Mode (0x02)
                    if (frame_buf[6] == 0x02) {
                        uint8_t state = frame_buf[8];
                        uint16_t moving_dist = frame_buf[9] | (frame_buf[10] << 8);
                        uint8_t moving_energy = frame_buf[11];
                        uint16_t stat_dist = frame_buf[12] | (frame_buf[13] << 8);
                        uint8_t stat_energy = frame_buf[14];
                        uint16_t detect_dist = frame_buf[15] | (frame_buf[16] << 8);

                        const char *state_str = "None";
                        if (state == 1) state_str = "Moving";
                        else if (state == 2) state_str = "Stationary";
                        else if (state == 3) state_str = "Both";

                        // *Verification:* Successful parse prints formatted target metrics to log
                        ESP_LOGI(TAG, "State: %-10s | Move: %4dcm (E:%3d) | Stat: %4dcm (E:%3d) | Detect: %4dcm",
                                 state_str, moving_dist, moving_energy, stat_dist, stat_energy, detect_dist);
                    }
                }
            }
        }
    }
    free(dtbuf);
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Application Starting up...");

    dummy_queue = xQueueCreate(10, sizeof(int));
    if (dummy_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create dummy queue");
        return;
    }

  
    ld2410cSetup();

    //xTaskCreate(&uartSenderTask, "uartSenderTask", 2048, NULL, 5, NULL);
    xTaskCreate(&ld2410cEventTask, "ld2410cEventTask", 4096, NULL, 5, NULL);
}
