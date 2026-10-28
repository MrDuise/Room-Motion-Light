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
    ESP_ERROR_CHECK(uart_set_pin(uart_num, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Install the driver
    ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
}

void uartLoopbackTask(void *pvParameter) {
    const char *test_str = "Hello ESP32 UART Loopback!\n";
    uint8_t data[BUF_SIZE];

    for(;;){
       uart_write_bytes(UART_NUM, test_str, strlen(test_str));
        ESP_LOGI("UART_TEST", "Sent: %s", test_str);

        // 2. Read data back from RX (GPIO 5) with a 1-second timeout
        int length = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(1000));
        
        if (length > 0) {
            data[length] = '\0'; // Null-terminate string safely
            ESP_LOGI("UART_TEST", "Received: %s", data);
        } else {
            ESP_LOGW("UART_TEST", "No data received (timeout)");
        }

        // Wait 2 seconds before the next iteration
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}


void sender_task(void *pvParameter) {
    int dummy_data = 0; 
    for(;;){
        dummy_data++;
        if(xQueueSend(dummy_queue, &dummy_data, pdMS_TO_TICKS(100)) == pdPASS) {
            ESP_LOGI(TAG, "Sent data: %d", dummy_data);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay for 100ms to avoid busy looping
    }
}

void receiver_task(void *pvParameter) {
    int recieved_data;
    for(;;){
        if(xQueueReceive(dummy_queue, &recieved_data, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Received data: %d", recieved_data);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Application Starting up...");

    // Create and kick off our background task


    dummy_queue = xQueueCreate(10, sizeof(int)); // Create a queue with 10 items, each item being an int

    /** 
     * create a task to start sending dummy data
     * create a taks to recieve dummy data from the sending task with queue
     * recieving tasks must block or pause when no data is available
     */
if(dummy_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create dummy queue");
    return;
}
    xTaskCreate(
        &sender_task,   // Function pointer
        "sender_task",  // Name of the task
        2048,           // Stack size allocated to the task (in bytes)
        NULL,           // Parameters passed to function
        5,              // Task Priority
        NULL            // Task handle
    );

    xTaskCreate(
        &receiver_task,   // Function pointer
        "receiver_task",  // Name of the task
        2048,             // Stack size allocated to the task (in bytes)
        NULL,             // Parameters passed to function
        5,                // Task Priority
        NULL              // Task handle
    );
    uartLoopbackSetup();
    xTaskCreate(
        &uartLoopbackTask,   // Function pointer
        "uartLoopbackTask",  // Name of the task
        2048,                // Stack size allocated to the task (in bytes)
        NULL,                // Parameters passed to function
        5,                   // Task Priority
        NULL                 // Task handle
    );
}
