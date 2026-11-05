#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_err.h"



#define UART_NUM UART_NUM_2
#define BUF_SIZE (1024)
// A unique tag identifier for our debug logs
static const char *TAG = "MAIN_APP";

QueueHandle_t dummy_queue;
QueueHandle_t uart_queue;




void uartLoopbackSetup(void) {
    const int uart_buffer_size = (1024 * 2);
    const uart_port_t uart_num = UART_NUM_2;

    uart_config_t uart_config = {
        .baud_rate = 115200,              // Any baud rate works, as long as TX/RX match
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // MUST be disabled for a simple TX-to-RX loopback
        .rx_flow_ctrl_thresh = 122,
    };
    
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // Route TX (IO4) to RX (IO5). 
    // Pass UART_PIN_NO_CHANGE for RTS and CTS since we aren't using them.
    ESP_ERROR_CHECK(uart_set_pin(uart_num, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Install the driver
    ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
}

// Dedicated periodic sender task
void uartSenderTask(void *pvParameter) {
    const char *test_str = "Hello ESP32 UART Async Event Loopback!\n";
    for (;;) {
        uart_write_bytes(UART_NUM, test_str, strlen(test_str));
        ESP_LOGI("UART_TEST", "Sent: %s", test_str);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds
    }
}

// Dedicated asynchronous event receiver task
void uartEventTask(void *pvParameter) {
    uart_event_t event;
    uint8_t *dtbuf = (uint8_t *)malloc(BUF_SIZE);

    for (;;) {
        // Block indefinitely until a UART event occurs
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            bzero(dtbuf, BUF_SIZE);
            switch (event.type) {
                case UART_DATA:
                    // Data was received; read it using event.size
                    uart_read_bytes(UART_NUM, dtbuf, event.size, portMAX_DELAY);
                    dtbuf[event.size] = '\0'; // Null-terminate safely
                    ESP_LOGI("UART_TEST", "Received [Async Event]: %s", dtbuf);
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW("UART_TEST", "Hardware FIFO overflow");
                    uart_flush_input(UART_NUM);
                    xQueueReset(uart_queue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGW("UART_TEST", "Ring buffer full");
                    uart_flush_input(UART_NUM);
                    xQueueReset(uart_queue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGE("UART_TEST", "Parity error detected");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGE("UART_TEST", "Frame error detected");
                    break;

                default:
                    ESP_LOGI("UART_TEST", "Unhandled UART event type: %d", event.type);
                    break;
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

    // Your existing dummy sender/receiver tasks
    // ...

    // Setup UART and start separate sender & event receiver tasks
    uartLoopbackSetup();

    xTaskCreate(&uartSenderTask, "uartSenderTask", 2048, NULL, 5, NULL);
    xTaskCreate(&uartEventTask, "uartEventTask", 4096, NULL, 5, NULL);
}
