/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "freertos/semphr.h"


#ifdef CONFIG_EXAMPLE_IPV4
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#else
#define HOST_IP_ADDR CONFIG_EXAMPLE_IPV6_ADDR
#endif

#define PORT CONFIG_EXAMPLE_PORT

static const char *TAG = "example";
static const char *payload = "Message from ESP32 ";

#define BUTTON_MAX_DECR_CNT 10u
#define BUTTON_MAX_EVT_CNT  10u 
#define BUTTON_PIN          GPIO_NUM_15 //pin IO15

typedef enum {
    BUTTON_EVT_DECR = 0,
    BUTTON_EVT_MAX
} button_evt_t; 

SemaphoreHandle_t cnt_decrement_smph = NULL; 
QueueHandle_t button_evt_queue = 0; 

static void button_isr(void * param) 
{
    if (!gpio_get_level(BUTTON_PIN))
    {
        BaseType_t  high_prio_task_unblocked = pdFALSE; 
        // xQueueSendFromISR(button_evt_queue, &BUTTON_EVT_DECR, &high_prio_task_unblocked)); 
        xSemaphoreGiveFromISR(cnt_decrement_smph, &high_prio_task_unblocked);
        if (high_prio_task_unblocked != pdFALSE) {
            portYIELD_FROM_ISR(); 
        }
    }
}

static void button_evts_init(void)
{
    cnt_decrement_smph = xSemaphoreCreateCounting(BUTTON_MAX_DECR_CNT, 0);
    esp_err_t smph_success = cnt_decrement_smph? ESP_OK : ESP_FAIL; 
    ESP_ERROR_CHECK(smph_success); 
    // button_evt_queue = xQueueCreate(BUTTON_MAX_EVT_CNT, sizeof(button_evt_t));
    // esp_err_t queue_success = button_evt_queue? ESP_OK : ESP_FAIL; 
    // ESP_ERROR_CHECK(queue_success); 
    ESP_ERROR_CHECK(gpio_pullup_en(BUTTON_PIN));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, &button_isr, NULL)); 
    ESP_ERROR_CHECK(gpio_intr_enable(BUTTON_PIN));
    ESP_ERROR_CHECK(gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE));
}

static void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {

#ifdef CONFIG_EXAMPLE_IPV4
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
#else // IPV6
        struct sockaddr_in6 dest_addr;
        inet6_aton(HOST_IP_ADDR, &dest_addr.sin6_addr);
        dest_addr.sin6_family = AF_INET6;
        dest_addr.sin6_port = htons(PORT);
        addr_family = AF_INET6;
        ip_protocol = IPPROTO_IPV6;
        inet6_ntoa_r(dest_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif

        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", HOST_IP_ADDR, PORT);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) {
            xSemaphoreTake(cnt_decrement_smph, portMAX_DELAY);
            ESP_LOGI(TAG, "Button pressed\n");
            int err = send(sock, payload, strlen(payload), 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }

            // int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // // Error occurred during receiving
            // if (len < 0) {
            //     ESP_LOGE(TAG, "recv failed: errno %d", errno);
            //     break;
            // }
            // // Data received
            // else {
            //     rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
            //     ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
            //     ESP_LOGI(TAG, "%s", rx_buffer);
            // }

            // vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    button_evts_init(); 

    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
}
