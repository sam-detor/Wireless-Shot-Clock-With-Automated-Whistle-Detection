#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "esp_mac.h"
#include "soc/soc.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"
#include "led_driver.h"

#define TEST_MODE 1
//#define WHISTLE_TEST_MODE 1

#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (APB_CLK_FREQ / TIMER_DIVIDER)  // convert counter value to seconds
#define RELOAD_VALUE          30

//COMAND CODES
#define REDUCE 0
#define RESET 3
#define ERROR 4

#define START_CHAR '1'
#define STOP_CHAR '2'
#define RESET_CHAR '3'
#define SPEC_STOP_CHAR '4'

struct Timer_State {
    bool stop_inst_recorded;
    bool timer_running;
    bool spec_stop;
    bool pending_reset;
};

static const char *TAG = "mqtts_example";
static QueueHandle_t s_timer_queue;
static gptimer_handle_t gptimer = NULL;
static gptimer_handle_t spec_gptimer = NULL;
//static bool stop_inst_recorded = false;
//static bool timer_running = false;
//static bool spec_stop = false;
//static bool pending_reset = false;
static struct Timer_State global_timer_state = {false,false,false,false};


//Cert for MQTT
extern const uint8_t mqtt_eclipseprojects_io_pem_start[]   asm("_binary_isrgrootx1_pem_start");
extern const uint8_t mqtt_eclipseprojects_io_pem_end[]   asm("_binary_isrgrootx1_pem_end");

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/game/#", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        switch(event->data[0])
        {
            case START_CHAR:
                gptimer_start(gptimer);
                global_timer_state.timer_running = true;
            break;
            case STOP_CHAR:
#ifdef WHISTLE_TEST_MODE
                printf("STOP COMMAND: time, %ld\n", xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
                gptimer_stop(gptimer);
                global_timer_state.timer_running = false;
                global_timer_state.spec_stop = false;
                global_timer_state.stop_inst_recorded = true;
#endif
            break;
            case RESET_CHAR:
                global_timer_state.pending_reset = true;
                uint32_t command_code = RESET;
                if (xQueueSend(s_timer_queue, &command_code, pdMS_TO_TICKS(10)) != pdPASS) {
                    ESP_LOGW(TAG, "Failed to send event: %ld\n", command_code);  // Queue might be full
                }
            break;
            case SPEC_STOP_CHAR:
#ifdef WHISTLE_TEST_MODE
                printf("SPEC STOP COMMAND: time, %ld\n", xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
                if (timer_running)
                {
                    global_timer_state.timer_state.stop_inst_recorded = false;
                    ESP_ERROR_CHECK(gptimer_stop(gptimer));
                    ESP_ERROR_CHECK(gptimer_set_raw_count(spec_gptimer,0));
                    ESP_ERROR_CHECK(gptimer_start(spec_gptimer));
                    global_timer_state.timer_state.spec_stop = true;
                }
#endif
                break;
            default:
                printf("ERROR: TOPIC=%.*s\r\n", event->topic_len, event->topic);
                printf("ERROR: DATA=%.*s\r\n", event->data_len, event->data);
            break;
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = "xxx",
            .verification.certificate = (const char *)mqtt_eclipseprojects_io_pem_start
            
        },
        .credentials = {
            .username = "xxx",
            .authentication = {
                .password = "xxx",
            },
        },
        .task = {
            .priority = 2,
        },
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

static bool timer_group_isr_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    uint32_t ulValueToSend = REDUCE;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Send the value to the queue from ISR
    xQueueSendFromISR(s_timer_queue, &ulValueToSend, &xHigherPriorityTaskWoken);

    return xHigherPriorityTaskWoken == pdTRUE;
}

static bool spec_timer_isr_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    ESP_ERROR_CHECK(gptimer_stop(spec_gptimer));
    uint32_t ulValueToSend = REDUCE;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    bool return_val = true;
    global_timer_state.spec_stop = false;

    if (!stop_inst_recorded)
    {
        uint64_t time_left;
        ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &time_left));
        if(time_left > 700000)
        {
            //uint32_t ulValueToSend = REDUCE;
            xQueueSendFromISR(s_timer_queue, &ulValueToSend, &xHigherPriorityTaskWoken);
            ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer, 300000 - (1000000 - time_left)));
            return_val = (xHigherPriorityTaskWoken == pdTRUE);
        }
        else
        {
            ESP_ERROR_CHECK(gptimer_set_raw_count(gptimer, time_left + 300000));
        }
        ESP_ERROR_CHECK(gptimer_start(gptimer));
    }

    return return_val;
}

void timer_post(void *pvParameters)
{
    uint32_t timer_state = RELOAD_VALUE;
    uint32_t received_event = ERROR;
    while (true)
    {
        if (xQueueReceive(s_timer_queue, &received_event, portMAX_DELAY)) {
            
            if(global_timer_state.pending_reset && received_event == REDUCE)
            {
                continue;
            }
            switch (received_event)
            {
                case REDUCE:
                    if(timer_state > 0)
                    {
                        timer_state--;
                    }
                break;
                case RESET:
                    gptimer_set_raw_count(gptimer,0);
                    timer_state = RELOAD_VALUE;
                    global_timer_state.pending_reset = false;
                break;
            }
#ifdef TEST_MODE
            printf("%ld\n",timer_state);
#elif !defined(WHISTLE_TEST_MODE)
            display_number(timer_state);
#endif
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_client", ESP_LOG_NONE);
    esp_log_level_set("mqtt_example", ESP_LOG_NONE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    s_timer_queue = xQueueCreate(10, sizeof(uint32_t));

    /* Setting up timer */
    const gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
        .intr_priority = 3,
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &spec_gptimer));
    
    const gptimer_alarm_config_t alarm_config = {
        .alarm_count = 1000000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer,&alarm_config));

    const gptimer_alarm_config_t spec_alarm_config = {
        .alarm_count = 300000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(spec_gptimer, &spec_alarm_config));

    const gptimer_event_callbacks_t callback_config = {
        .on_alarm = timer_group_isr_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &callback_config, NULL));

    const gptimer_event_callbacks_t spec_callback_config = {
        .on_alarm = spec_timer_isr_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(spec_gptimer, &spec_callback_config, NULL));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_enable(spec_gptimer));

#if !defined(TEST_MODE) && !defined(WHISTLE_TEST_MODE)
    led_matrix_config();
#endif

    xTaskCreate(timer_post, "Task Read Queue", 2048, NULL, 1, NULL);

    mqtt_app_start();
}
