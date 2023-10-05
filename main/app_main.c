#include <stdio.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_spi_flash.h>
#include <esp_log.h>

#include "nblink.h"

static const char* TAG = "NBLINK";

gpio_num_t NBLK_LED1 = GPIO_NUM_21;
gpio_num_t NBLK_LED2 = GPIO_NUM_22;
gpio_num_t NBLK_LED3 = GPIO_NUM_23;

void example_blink_params()
{
  ESP_LOGI(TAG, "Params example");
  nblk_start(NBLK_LED1, 2000, 60000, 0, 1);
  vTaskDelay(pdMS_TO_TICKS(15000));
  nblk_start(NBLK_LED1, 500, 30000, 1, 2); // Override (higher prio)
  nblk_start(NBLK_LED1, 250, 10000, 0, 0); // Doesn't do anything (lower prio)
}

void example_no_sync()
{
  ESP_LOGI(TAG, "No sync example");
  nblk_start(NBLK_LED1, 2000, NBLK_FOREVER, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(300));
  nblk_start(NBLK_LED2, 2000, NBLK_FOREVER, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(700));
  nblk_start(NBLK_LED3, 3000, NBLK_FOREVER, 0, 0);
}

static nblk_mgr_t mgr;
void example_with_sync()
{
  ESP_LOGI(TAG, "Sync example");
  nblk_sync_create_mgr(&mgr, 1000, 1);
  nblk_sync_start(&mgr, NBLK_LED1, 2000, NBLK_FOREVER, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(300));
  nblk_sync_start(&mgr, NBLK_LED2, 2000, NBLK_FOREVER, 0, 0);
  vTaskDelay(pdMS_TO_TICKS(700));
  nblk_sync_start(&mgr, NBLK_LED3, 3000, NBLK_FOREVER, 0, 0);
}

void app_main(void)
{
  // Choose one of the example

  // example_blink_params();
  example_no_sync();
  // example_with_sync();
}
