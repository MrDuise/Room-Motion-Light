#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"

// A unique tag identifier for our debug logs
static const char *TAG = "MAIN_APP";

QueueHandle_t dummy_queue;



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
    
}
