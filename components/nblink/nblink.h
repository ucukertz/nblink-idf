#ifndef __NBLINK_H
#define __NBLINK_H

#include <stdint.h>
#include <string.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_log.h>

#define NBLK_FOREVER UINT32_MAX

typedef struct {
  gpio_num_t gpio;
  uint32_t period_ms;
  uint32_t remaining_ms;
  bool level;
  bool stop_level;
  uint8_t priority;
  esp_timer_handle_t tmr_blinker;
} nblink_t;

typedef struct {
  uint8_t nblk_active;
  nblink_t* nblk_list;
  uint32_t sync_tbase_ms;
  bool sync_level;
  SemaphoreHandle_t smpr_list_modify;
  esp_timer_handle_t tmr_blinker;
  volatile bool deleting;
} nblk_mgr_t;

#ifdef __cplusplus
extern "C" {
#endif
/* Regular non-blocking blinking */

bool nblk_start(gpio_num_t gpio, uint32_t pd_ms, uint32_t dr_ms, bool sl, uint8_t prio);
bool nblk_is_blinking(gpio_num_t gpio);
bool nblk_stop(gpio_num_t gpio, bool sl);

/* Synchronized non-blocking blinking */

bool nblk_sync_create_mgr(nblk_mgr_t* mgr, uint32_t tbase_ms, bool sync_level);
void nblk_sync_delete_mgr(nblk_mgr_t* mgr);
bool nblk_sync_start(nblk_mgr_t* mgr, gpio_num_t gpio, uint32_t pd_ms, uint32_t dr_ms, bool sl, uint8_t prio);
bool nblk_is_sync_blinking(nblk_mgr_t* mgr, gpio_num_t gpio);
bool nblk_sync_stop(nblk_mgr_t* mgr, gpio_num_t gpio, bool sl);

#ifdef __cplusplus
}
#endif

#endif // __NBLINK_H