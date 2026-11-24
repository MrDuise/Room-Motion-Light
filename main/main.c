#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "driver/gpio.h"

#define LED_PIN (2)  // Assuming the LED is connected to GPIO 2



#define UART_NUM UART_NUM_2
#define BUF_SIZE (1024)
#define TXD_PIN (17)
#define RXD_PIN (16)

#define LD2410C_FRAME_LEN 23

typedef enum {
    LD2410C_STATE_NONE = 0,
    LD2410C_STATE_MOVING = 1,
    LD2410C_STATE_STATIONARY = 2,
    LD2410C_STATE_BOTH = 3
} ld2410c_target_state_t;

typedef struct {
    bool valid;
    uint8_t state;            // 0: None, 1: Moving, 2: Stationary, 3: Both
    uint16_t moving_dist_cm;
    uint8_t moving_energy;
    uint16_t stat_dist_cm;
    uint8_t stat_energy;
    uint16_t detect_dist_cm;
} ld2410c_target_data_t;


static const char *ld2410c_state_to_str(uint8_t state) {
    switch (state) {
        case LD2410C_STATE_MOVING:     return "Moving";
        case LD2410C_STATE_STATIONARY: return "Stationary";
        case LD2410C_STATE_BOTH:       return "Both";
        default:                       return "None";
    }
}

// A unique tag identifier for our debug logs
static const char *TAG = "MAIN_APP";
//mostly likely needs a different name than the one in main.c, but for now, let's keep it simple and use the same name.
// static const char *TAG = "LD2410";

static QueueHandle_t radar_state_queue = NULL;
QueueHandle_t uart_queue;

bool ld2410c_parse_frame(const uint8_t frame[LD2410C_FRAME_LEN], ld2410c_target_data_t *out);

void ledSetup(void) {
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

void ledOn(void) {
    gpio_set_level(LED_PIN, 1);
}

void ledOff(void) {
    gpio_set_level(LED_PIN, 0);
}


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
// void ld2410cEventTask(void *pvParameter) {
//     uart_event_t event;
//     uint8_t *dtbuf = (uint8_t *)malloc(BUF_SIZE);
//     uint8_t data[BUF_SIZE];
//     uint8_t frame_buf[23];


//     //need to separate the reading of the data from the parsing of the data, so that the parser can be tested independently of the UART driver. The parser will be a separate function that takes in a buffer and returns a struct with the parsed data.
    
//     for (;;) {
//        // Read incoming bytes from the sensor (timeout after 20ms)
//         int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, pdMS_TO_TICKS(20));
        
//         if (len > 0) {
//             for (int i = 0; i < len; i++) {
//                 // Shift frame buffer left by 1 byte and insert the newest byte
//                 for (int j = 0; j < 22; j++) {
//                     frame_buf[j] = frame_buf[j + 1];
//                 }
//                 frame_buf[22] = data[i];

//                 // Check for Header (F4 F3 F2 F1) and Footer (F8 F7 F6 F5)
//                 if (frame_buf[0] == 0xF4 && frame_buf[1] == 0xF3 && 
//                     frame_buf[2] == 0xF2 && frame_buf[3] == 0xF1 &&
//                     frame_buf[19] == 0xF8 && frame_buf[20] == 0xF7 && 
//                     frame_buf[21] == 0xF6 && frame_buf[22] == 0xF5) {
                    
//                     // Verify data type is Basic Reporting Mode (0x02)
//                     if (frame_buf[6] == 0x02) {
//                         uint8_t state = frame_buf[8];
//                         uint16_t moving_dist = frame_buf[9] | (frame_buf[10] << 8);
//                         uint8_t moving_energy = frame_buf[11];
//                         uint16_t stat_dist = frame_buf[12] | (frame_buf[13] << 8);
//                         uint8_t stat_energy = frame_buf[14];
//                         uint16_t detect_dist = frame_buf[15] | (frame_buf[16] << 8);

//                         const char *state_str = "None";
//                         if (state == 1) state_str = "Moving";
//                         else if (state == 2) state_str = "Stationary";
//                         else if (state == 3) state_str = "Both";

//                         // *Verification:* Successful parse prints formatted target metrics to log
//                         ESP_LOGI(TAG, "State: %-10s | Move: %4dcm (E:%3d) | Stat: %4dcm (E:%3d) | Detect: %4dcm",
//                                  state_str, moving_dist, moving_energy, stat_dist, stat_energy, detect_dist);
//                     }
//                 }
//             }
//         }
//     }
//     free(dtbuf);
//     vTaskDelete(NULL);
// }

void ld2410c_log_data(const ld2410c_target_data_t *d) {
    if (d == NULL || !d->valid) {
        return;
    }

    ESP_LOGI(TAG, "State: %-10s | Move: %4dcm (E:%3d) | Stat: %4dcm (E:%3d) | Detect: %4dcm",
             ld2410c_state_to_str(d->state),
             d->moving_dist_cm, d->moving_energy,
             d->stat_dist_cm, d->stat_energy,
             d->detect_dist_cm);
}



void ld2410cEventTask(void *pvParameter) {
    (void)pvParameter;

    uint8_t data[BUF_SIZE];
    uint8_t frame_buf[LD2410C_FRAME_LEN] = {0};
    ld2410c_target_data_t parsed;

    for (;;) {
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, pdMS_TO_TICKS(20));

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                // Shift left by 1 byte and append newest byte
                for (int j = 0; j < LD2410C_FRAME_LEN - 1; j++) {
                    frame_buf[j] = frame_buf[j + 1];
                }
                frame_buf[LD2410C_FRAME_LEN - 1] = data[i];

                if (ld2410c_parse_frame(frame_buf, &parsed)) {
                    ld2410c_log_data(&parsed);

                    // Send to state task; overwrite oldest unread item if queue is full
                    if (radar_state_queue != NULL &&
                        xQueueSend(radar_state_queue, &parsed, 0) != pdTRUE) {
                        ld2410c_target_data_t dropped;
                        xQueueReceive(radar_state_queue, &dropped, 0);
                        xQueueSend(radar_state_queue, &parsed, 0);
                    }
                }
            }
        }
    }

    vTaskDelete(NULL);
}


// Pure parser: no hardware, logging, or RTOS dependencies (easy to unit test)
bool ld2410c_parse_frame(const uint8_t frame[LD2410C_FRAME_LEN], ld2410c_target_data_t *out) {
    if (frame == NULL || out == NULL) {
        return false;
    }

    out->valid = false;

    // Check Header (F4 F3 F2 F1) and Footer (F8 F7 F6 F5)
    bool has_header = (frame[0] == 0xF4 && frame[1] == 0xF3 &&
                       frame[2] == 0xF2 && frame[3] == 0xF1);
    bool has_footer = (frame[19] == 0xF8 && frame[20] == 0xF7 &&
                       frame[21] == 0xF6 && frame[22] == 0xF5);

    if (!has_header || !has_footer) {
        return false;
    }

    // Verify data type is Basic Reporting Mode (0x02)
    if (frame[6] != 0x02) {
        return false;
    }

    out->state          = frame[8];
    out->moving_dist_cm = (uint16_t)(frame[9]  | (frame[10] << 8));
    out->moving_energy  = frame[11];
    out->stat_dist_cm   = (uint16_t)(frame[12] | (frame[13] << 8));
    out->stat_energy    = frame[14];
    out->detect_dist_cm = (uint16_t)(frame[15] | (frame[16] << 8));
    out->valid          = true;

    return true;
}


//this is my state task, or status. its the check that holds the current system state, of 
//idle, triggered_lit, night armed, and cooldown
// Consumes parsed sensor data from the queue and drives system state
void stateControlTask(void *pvParameter) {
    (void)pvParameter;

    ld2410c_target_data_t rx_data;

    for (;;) {
        if (radar_state_queue != NULL &&
            xQueueReceive(radar_state_queue, &rx_data, portMAX_DELAY) == pdTRUE) {

            if (!rx_data.valid) {
                continue;
            }

            // Example system state decisions based on presence/distance
            switch (rx_data.state) {
                case LD2410C_STATE_MOVING:
                case LD2410C_STATE_BOTH:
                    // Active presence detected -> e.g., turn on active/occupied state
                    //this turns on the light
                    ledOn();
                    break;

                case LD2410C_STATE_STATIONARY:
                    // Still presence detected -> e.g., keep occupied / avoid premature timeout
                    ledOff();
                    break;

                case LD2410C_STATE_NONE:
                default:
                    // No target detected -> e.g., start vacancy/idle timer
                    ledOff();
                    break;
            }
        }
    }

    vTaskDelete(NULL);
}

void ld2410cStartTasks(void) {
    if (radar_state_queue == NULL) {
        radar_state_queue = xQueueCreate(10, sizeof(ld2410c_target_data_t));
        configASSERT(radar_state_queue != NULL);
    }

    xTaskCreate(ld2410cEventTask, "ld2410c_uart_task", 4096, NULL, 5, NULL);
    xTaskCreate(stateControlTask, "state_ctrl_task", 4096, NULL, 5, NULL);
}


void app_main(void) {
    ESP_LOGI(TAG, "Application Starting up...");
    ld2410cSetup();
    ledSetup();
    ledOff();  // Ensure the LED is off initially
    ld2410cStartTasks();
   
}
