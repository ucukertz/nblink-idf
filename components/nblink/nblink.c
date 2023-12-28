#include "nblink.h"

static const char* TAG = "nblk";
static const uint32_t NBLK_MINIMUM_PERIOD = 2*portTICK_PERIOD_MS;

static SemaphoreHandle_t smpr_list_modify = NULL;
static uint8_t nblk_active = 0;
static nblink_t* nblk_list = NULL;

/* Regular non-blocking blinking */

static uint8_t get_nblk_idx_in_list(gpio_num_t gpio)
{
  if(!nblk_list) return UINT8_MAX;
  for (int i = 0; i<nblk_active; i++) {
    if (nblk_list[i].gpio == gpio)
    return i;
  }
  return UINT8_MAX;
}

static void add_nblk_to_list(nblink_t nblk)
{
  xSemaphoreTake(smpr_list_modify, portMAX_DELAY);
  uint8_t idx = get_nblk_idx_in_list(nblk.gpio);
  if (idx == UINT8_MAX) {
    nblk_active++;
    if (nblk_list == NULL)
    nblk_list = malloc(nblk_active*sizeof(nblink_t));
    else nblk_list = realloc(nblk_list, nblk_active*sizeof(nblink_t));
    memcpy(&nblk_list[nblk_active-1], &nblk, sizeof(nblink_t));
  }
  xSemaphoreGive(smpr_list_modify);
}

static void del_nblk_from_list(gpio_num_t gpio)
{
  xSemaphoreTake(smpr_list_modify, portMAX_DELAY);
  uint8_t idx = get_nblk_idx_in_list(gpio);
  if (idx != UINT8_MAX) {
    nblk_active--;
    gpio_set_level(gpio, nblk_list[idx].stop_level);
    ESP_LOGD(TAG, "GPIO%u stopped at level %u", gpio, nblk_list[idx].stop_level);
    esp_timer_stop(nblk_list[idx].tmr_blinker);
    esp_timer_delete(nblk_list[idx].tmr_blinker);
    if (nblk_active > 0) {
      memcpy(&nblk_list[idx], &nblk_list[idx+1], (nblk_active-idx)*sizeof(nblink_t));
      nblk_list = realloc(nblk_list, nblk_active*sizeof(nblink_t));
    }
    else {
      free(nblk_list);
      nblk_list = NULL;
    }
  }
  xSemaphoreGive(smpr_list_modify);
}

static void nblk_cb(void* arg)
{
  xSemaphoreTake(smpr_list_modify, portMAX_DELAY);

  gpio_num_t gpio = (gpio_num_t)arg;
  uint8_t idx = get_nblk_idx_in_list(gpio);

  if (nblk_list[idx].period_ms/2 < nblk_list[idx].remaining_ms) {
    if (nblk_list[idx].remaining_ms != NBLK_FOREVER)
    nblk_list[idx].remaining_ms -= nblk_list[idx].period_ms/2;
    if (nblk_list[idx].level) nblk_list[idx].level = 0;
    else nblk_list[idx].level = 1;
    gpio_set_level(nblk_list[idx].gpio, nblk_list[idx].level);
    ESP_LOGD(TAG, "GPIO%u set to %u", nblk_list[idx].gpio, nblk_list[idx].level);
    xSemaphoreGive(smpr_list_modify);
  }
  else {
    xSemaphoreGive(smpr_list_modify);
    del_nblk_from_list(gpio);
  }
}

/**
 * @brief Start non-blocking blink on GPIO
 * @note Blink is executed by ESP timer task
 * @param gpio GPIO to blink
 * @param pd_ms Blink period
 * @param dr_ms Blink duration
 * @param sl Stop level. Set GPIO to this level when blinking ends.
 * @param prio Priority. Lower priority blinking on same GPIO always gets overidden.
 *        Same priority blinking on same GPIO are overidden if its remaining duration is shorter.
 * @return true if new non-blocking blink is started successfully
 */
bool nblk_start(gpio_num_t gpio, uint32_t pd_ms, uint32_t dr_ms, bool sl, uint8_t prio)
{
  if (pd_ms < NBLK_MINIMUM_PERIOD) {
    ESP_LOGE(TAG, "Blink period too short. GPIO: %u Minimum: %ums", gpio, NBLK_MINIMUM_PERIOD);
    return 0;
  }
  else if (dr_ms%pd_ms != 0 && dr_ms != NBLK_FOREVER) {
    ESP_LOGE(TAG, "Blink duration is not multiple of blink period. GPIO: %u", gpio);
    return 0;
  }

  if (!smpr_list_modify) {
    smpr_list_modify = xSemaphoreCreateMutex();
  }

  uint8_t idx = get_nblk_idx_in_list(gpio);
  if (idx != UINT8_MAX) {
    if (prio > nblk_list[idx].priority) {
      del_nblk_from_list(gpio);
    }
    else if (prio == nblk_list[idx].priority && dr_ms > nblk_list[idx].remaining_ms) {
      del_nblk_from_list(gpio);
    }
    else {
      return 0;
    }
  }

  nblink_t nblk = {0};
  nblk.gpio = gpio;
  nblk.period_ms = pd_ms;
  nblk.remaining_ms = dr_ms;
  nblk.stop_level = sl;
  nblk.priority = prio;

  add_nblk_to_list(nblk);
  xSemaphoreTake(smpr_list_modify, portMAX_DELAY);
  idx = get_nblk_idx_in_list(gpio);

  esp_timer_create_args_t targs_blinker;
  targs_blinker.callback = nblk_cb;
  targs_blinker.arg = (void*)gpio;
  targs_blinker.dispatch_method = ESP_TIMER_TASK;
  esp_timer_create(&targs_blinker, &nblk_list[idx].tmr_blinker);
  esp_timer_start_periodic(nblk_list[idx].tmr_blinker, (pd_ms/2)*1e3);
  xSemaphoreGive(smpr_list_modify);

  return 1;
}

/**
 * @brief Check if GPIO is blinking with no sync
 * @param gpio GPIO to check
 * @return true if GPIO is blinking
 */
bool nblk_is_blinking(gpio_num_t gpio)
{
  uint8_t idx = get_nblk_idx_in_list(gpio);
  if (idx != UINT8_MAX) return 1;
  else return 0;
}

/**
 * @brief Stop non-blocking blink on GPIO
 * @param gpio GPIO to stop
 * @param sl Stop level. Set GPIO to this level when blinking ends.
 * @return true if non-blocking blink is stopped successfully
 */
bool nblk_stop(gpio_num_t gpio, bool sl)
{
  if (!smpr_list_modify) return 0;

  xSemaphoreTake(smpr_list_modify, portMAX_DELAY);
  uint8_t idx = get_nblk_idx_in_list(gpio);
  ESP_LOGD(TAG, "Stopping GPIO%u. idx: %u", gpio, idx);
  if (idx == UINT8_MAX) {
    xSemaphoreGive(smpr_list_modify);
    return 0;
  }
  xSemaphoreGive(smpr_list_modify);
  del_nblk_from_list(gpio);
  gpio_set_level(gpio, sl);
  ESP_LOGD(TAG, "GPIO%u stopped at level %u", gpio, sl);
  return 1;
}

/* Synchronized non-blocking blinking */

static uint8_t get_nblk_idx_in_list_s(nblk_mgr_t* mgr, gpio_num_t gpio)
{
  if (!mgr->nblk_list) return UINT8_MAX;
  for (int i = 0; i < mgr->nblk_active; i++) {
    if (mgr->nblk_list[i].gpio == gpio)
    return i;
  }
  return UINT8_MAX;
}

static void add_nblk_to_list_s(nblk_mgr_t* mgr, nblink_t nblk)
{
  xSemaphoreTake(mgr->smpr_list_modify, portMAX_DELAY);
  uint8_t idx = get_nblk_idx_in_list_s(mgr, nblk.gpio);
  if (idx == UINT8_MAX) {
    mgr->nblk_active++;
    if (mgr->nblk_list == NULL)
    mgr->nblk_list = malloc(mgr->nblk_active*sizeof(nblink_t));
    else mgr->nblk_list = realloc(mgr->nblk_list, mgr->nblk_active*sizeof(nblink_t));
    memcpy(&mgr->nblk_list[mgr->nblk_active-1], &nblk, sizeof(nblink_t));

    // Synchronize
    for (idx = 0; idx < mgr->nblk_active; idx++) {
      gpio_set_level(mgr->nblk_list[idx].gpio, mgr->sync_level);
      mgr->nblk_list[idx].level = mgr->sync_level;
      ESP_LOGD(TAG, "GPIO%u sync at level %u", mgr->nblk_list[idx].gpio, mgr->sync_level);

      uint32_t correction = mgr->nblk_list[idx].remaining_ms%mgr->nblk_list[idx].period_ms;
      mgr->nblk_list[idx].remaining_ms -= correction;
      if (correction)
      mgr->nblk_list[idx].remaining_ms += mgr->nblk_list[idx].period_ms;
    }
  }
  xSemaphoreGive(mgr->smpr_list_modify);
}

static void del_nblk_from_list_s(nblk_mgr_t* mgr, gpio_num_t gpio)
{
  xSemaphoreTake(mgr->smpr_list_modify, portMAX_DELAY);
  uint8_t idx = get_nblk_idx_in_list_s(mgr, gpio);
  if (idx != UINT8_MAX) {
    mgr->nblk_active--;
    gpio_set_level(gpio, mgr->nblk_list[idx].stop_level);
    ESP_LOGD(TAG, "GPIO%u stopped at level %u", gpio, mgr->nblk_list[idx].stop_level);
    if (mgr->nblk_active > 0) {
      memcpy(&mgr->nblk_list[idx], &mgr->nblk_list[idx+1], (mgr->nblk_active-idx)*sizeof(nblink_t));
      mgr->nblk_list = realloc(mgr->nblk_list, mgr->nblk_active*sizeof(nblink_t));
    }
    else {
      free(mgr->nblk_list);
      mgr->nblk_list = NULL;
    }
  }
  xSemaphoreGive(mgr->smpr_list_modify);
}

static void nblk_sync_cb(void* arg)
{
  nblk_mgr_t* mgr = (nblk_mgr_t*)arg;

  if (!mgr->nblk_active) return;

  xSemaphoreTake(mgr->smpr_list_modify, portMAX_DELAY);
  for (uint8_t idx = 0; idx < mgr->nblk_active; idx++) {
    if (mgr->nblk_list[idx].remaining_ms != NBLK_FOREVER)
    mgr->nblk_list[idx].remaining_ms -= mgr->sync_tbase_ms/2;

    if (mgr->nblk_list[idx].remaining_ms%(mgr->nblk_list[idx].period_ms/2) == 0) {
      if (mgr->nblk_list[idx].level) mgr->nblk_list[idx].level = 0;
      else mgr->nblk_list[idx].level = 1;
      gpio_set_level(mgr->nblk_list[idx].gpio, mgr->nblk_list[idx].level);
      ESP_LOGD(TAG, "GPIO%u set to %u", mgr->nblk_list[idx].gpio, mgr->nblk_list[idx].level);
    }
  }
  xSemaphoreGive(mgr->smpr_list_modify);

  for (uint8_t idx = 0; idx < mgr->nblk_active; idx++) {
    if (mgr->nblk_list[idx].period_ms/2 > mgr->nblk_list[idx].remaining_ms)
    del_nblk_from_list_s(mgr, mgr->nblk_list[idx].gpio);
  }
}

/**
 * @brief Create a manager for synchronized non-blocking blinking
 * @param mgr [out] Storage for created manager
 * @param tbase_ms Manager timebase
 * @note Use highest common factor of all blinking period that would be managed by a manager
 *       for optimal CPU utilization
 * @note Timebase can't be changed. Delete the manager and create a new one instead.
 * @param sync_level Blinking are synchronized to this level
 * @return true if manager is created successfully
 */
bool nblk_sync_create_mgr(nblk_mgr_t* mgr, uint32_t tbase_ms, bool sync_level)
{
  if (tbase_ms < NBLK_MINIMUM_PERIOD) {
    ESP_LOGE(TAG, "Timebase too short. Minimum: %ums", NBLK_MINIMUM_PERIOD);
    return 0;
  }

  mgr->nblk_list = NULL;
  mgr->nblk_active = 0;
  mgr->smpr_list_modify = xSemaphoreCreateMutex();
  mgr->sync_tbase_ms = tbase_ms;
  mgr->sync_level = sync_level;

  esp_timer_create_args_t targs_blinker;
  targs_blinker.callback = nblk_sync_cb;
  targs_blinker.arg = (void*)mgr;
  targs_blinker.dispatch_method = ESP_TIMER_TASK;
  esp_timer_create(&targs_blinker, &mgr->tmr_blinker);
  esp_timer_start_periodic(mgr->tmr_blinker, (tbase_ms/2)*1e3);

  return 1;
}

/**
 * @brief Delete a manager for synchronized non-blocking blinking
 * @note All GPIO being managed are stopped to their stop level
 * @param mgr Manager to delete
 */
void nblk_sync_delete_mgr(nblk_mgr_t* mgr)
{
  mgr->deleting = 1;
  esp_timer_stop(mgr->tmr_blinker);
  esp_timer_delete(mgr->tmr_blinker);

  uint8_t nblk_on_deletion = mgr->nblk_active;

  for (uint8_t i=0; i<nblk_on_deletion; i++)
  nblk_sync_stop(mgr, mgr->nblk_list[0].gpio, mgr->nblk_list[0].stop_level);

  if (mgr->nblk_list != NULL) free(mgr->nblk_list);
  vSemaphoreDelete(mgr->smpr_list_modify);
  mgr->smpr_list_modify = NULL;
}

/**
 * @brief Start synchronized non-blocking blink on GPIO
 * @note Blink is executed by ESP timer task
 * @param mgr Manager to use
 * @param gpio GPIO to blink
 * @param pd_ms Blink period
 * @param dr_ms Blink duration
 * @param sl Stop level. Set GPIO to this level when blinking ends.
 * @param prio Priority. Lower priority blinking on same GPIO always gets overidden.
 *        Same priority blinking on same GPIO are overidden if its remaining duration is shorter.
 * @return true if new non-blocking blink is started successfully
 */
bool nblk_sync_start(nblk_mgr_t* mgr, gpio_num_t gpio, uint32_t pd_ms, uint32_t dr_ms, bool sl, uint8_t prio)
{
  if (!mgr->smpr_list_modify) return 0;
  if (mgr->deleting) return 0;

  if (dr_ms == NBLK_FOREVER) dr_ms -= pd_ms;
  uint32_t correction = dr_ms%pd_ms;
  dr_ms -= correction;
  if (correction) dr_ms += pd_ms;

  if (pd_ms < NBLK_MINIMUM_PERIOD) {
    ESP_LOGE(TAG, "Blink period too short. GPIO: %u Minimum: %ums", gpio, NBLK_MINIMUM_PERIOD);
    return 0;
  }
  else if (pd_ms%mgr->sync_tbase_ms != 0) {
    ESP_LOGE(TAG, "Blink period is not multiple of sync manager timebase. GPIO: %u", gpio);
    return 0;
  }
  else if (!mgr->smpr_list_modify) {
    ESP_LOGE(TAG, "Sync manager not initialized");
    return 0;
  }

  uint8_t idx = get_nblk_idx_in_list_s(mgr, gpio);

  if (idx != UINT8_MAX) {
    if (prio > mgr->nblk_list[idx].priority) {
      del_nblk_from_list_s(mgr, gpio);
    }
    else if (prio == mgr->nblk_list[idx].priority && dr_ms > mgr->nblk_list[idx].remaining_ms) {
      del_nblk_from_list_s(mgr, gpio);
    }
    else {
      return 0;
    }
  }

  nblink_t nblk = {0};
  nblk.gpio = gpio;
  nblk.period_ms = pd_ms;
  nblk.remaining_ms = dr_ms;
  nblk.stop_level = sl;
  nblk.priority = prio;

  if (!mgr->deleting)
  add_nblk_to_list_s(mgr, nblk);
  return 1;
}

/**
 * @brief Check if GPIO is blinking with sync
 * @param mgr Manager to check
 * @param gpio GPIO to check
 * @return true if GPIO sync blink is being managed by the manager
 */
bool nblk_is_sync_blinking(nblk_mgr_t* mgr, gpio_num_t gpio)
{
  uint8_t idx = get_nblk_idx_in_list_s(mgr, gpio);
  if (idx != UINT8_MAX) return 1;
  else return 0;
}

/**
 * @brief Stop synchronized non-blocking blink on GPIO
 * @param mgr Manager used to start synchronized non-blocking blinking
 * @param gpio GPIO to stop
 * @param sl Stop level. Set GPIO to this level when blinking ends.
 * @return true if non-blocking blink is stopped successfully
 */
bool nblk_sync_stop(nblk_mgr_t* mgr, gpio_num_t gpio, bool sl)
{
  if (!mgr->smpr_list_modify) return 0;

  xSemaphoreTake(mgr->smpr_list_modify, portMAX_DELAY);
  uint8_t idx = get_nblk_idx_in_list_s(mgr, gpio);
  ESP_LOGD(TAG, "Stopping GPIO%u. idx: %u", gpio, idx);
  if (idx == UINT8_MAX) {
    xSemaphoreGive(mgr->smpr_list_modify);
    return 0;
  }
  xSemaphoreGive(mgr->smpr_list_modify);
  del_nblk_from_list_s(mgr, gpio);
  gpio_set_level(gpio, sl);
  ESP_LOGD(TAG, "GPIO%u stopped at level %u", gpio, sl);
  return 1;
}