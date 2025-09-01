# nblink-idf

**Non-blocking GPIO blink component for ESP-IDF**

A lightweight, thread-safe ESP-IDF component for non-blocking GPIO blinking with support for both individual and synchronized blinking patterns.

## Features

- **Non-blocking blinking**: Control multiple GPIOs without delaying your main application
- **Priority system**: Higher priority blinks can override lower ones on the same GPIO
- **Synchronized blinking**: Coordinate multiple GPIOs to blink in perfect sync
- **Configurable duration**: Set blink duration or blink forever
- **Thread-safe**: Uses semaphores for safe operation in a multi-tasking environment
- **ESP-IDF integration**: Designed as a component for easy integration into ESP-IDF projects

## Installation

### Prerequisites

- [ESP-IDF](https://github.com/espressif/esp-idf) v4.0 or later
- ESP32 development board

### Adding to Your Project

1.  Clone this repository:

    ```sh
    git clone [https://github.com/ucukertz/nblink-idf](https://github.com/ucukertz/nblink-idf) nblink
    ```

2.  Add the nblink component of this repository to component folder of your project:

    ```sh
    cp -r nblink/components/nblink /path/to/your/esp-idf-project/components/
    ```
'/path/to/your/esp-idf-project' should be replaced with the actual path to your ESP-IDF project


3.  Include the header in your project's source code:

    ```c
    #include "nblink.h"
    ```

## Usage

### Basic Non-blocking Blink

```c
#include "nblink.h"
#include "driver/gpio.h"

void app_main() {
    // Configure GPIO
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_2);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    
    // Start blinking: GPIO 2, period 500ms, duration 5000ms, stop at LOW, priority 1
    nblk_start(GPIO_NUM_2, 500, 5000, false, 1);
    
    // Run other logic or vTaskDelay()
    while(1) {
        // Your application code here
        vTaskDelay(100 / portTICK_PERIOD_MS);
        
        // Check if blinking
        if (nblk_is_blinking(GPIO_NUM_2)) {
            printf("GPIO 2 is blinking\n");
        }
    }
    
    // Stop blinking after some time
    nblk_stop(GPIO_NUM_2, false);
}
```

### Synchronized Blinking

```c
#include "nblink.h"
#include "driver/gpio.h"

void app_main() {
    // Configure GPIOs
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_4);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    
    // Create synchronized blink manager
    nblk_mgr_t sync_mgr;
    nblk_sync_create_mgr(&sync_mgr, 100, true);  // 100ms timebase, sync to HIGH
    
    // Start synchronized blinking
    nblk_sync_start(&sync_mgr, GPIO_NUM_2, 500, 5000, false, 1);
    nblk_sync_start(&sync_mgr, GPIO_NUM_4, 300, 5000, false, 1);
    
    // Both GPIOs will blink in perfect synchronization
    // Run other logic or vTaskDelay()
    for(int i = 0; i < 100; i++) {
        // Your application code here
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    // Clean up when done
    nblk_sync_delete_mgr(&sync_mgr);
}
```

## API Reference

### Regular Non-blocking Blinking

#### `nblk_start(gpio_num_t gpio, uint32_t pd_ms, uint32_t dr_ms, bool sl, uint8_t prio)`

Start non-blocking blink on a GPIO.

-   **gpio**: GPIO number to blink
-   **pd\_ms**: Blink period in milliseconds (must be ≥ `2*portTICK_PERIOD_MS`)
-   **dr\_ms**: Blink duration in milliseconds (use `NBLK_FOREVER` for infinite)
-   **sl**: Stop level (true=HIGH, false=LOW) when blinking ends
-   **prio**: Priority (0-255). Higher priority overrides lower on same GPIO
-   **Returns**: `true` if successful, `false` if failed

#### `nblk_is_blinking(gpio_num_t gpio)`

Check if a GPIO is currently blinking.

-   **gpio**: GPIO number to check
-   **Returns**: `true` if blinking, `false` if not

#### `nblk_stop(gpio_num_t gpio, bool sl)`

Stop blinking on a GPIO.

-   **gpio**: GPIO number to stop
-   **sl**: Stop level to set (true=HIGH, false=LOW)
-   **Returns**: `true` if successful, `false` if failed

### Synchronized Non-blocking Blinking

#### `nblk_sync_create_mgr(nblk_mgr_t* mgr, uint32_t tbase_ms, bool sync_level)`

Create a manager for synchronized blinking.

-   **mgr**: Pointer to manager structure
-   **tbase\_ms**: Timebase in milliseconds (use HCF of all periods for optimal performance)
-   **sync\_level**: Synchronization level (true=HIGH, false=LOW)
-   **Returns**: `true` if successful, `false` if failed

#### `nblk_sync_delete_mgr(nblk_mgr_t* mgr)`

Delete a synchronized blink manager.

-   **mgr**: Pointer to manager structure

#### `nblk_sync_start(nblk_mgr_t* mgr, gpio_num_t gpio, uint32_t pd_ms, uint32_t dr_ms, bool sl, uint8_t prio)`

Start synchronized blinking on a GPIO.

-   **mgr**: Pointer to manager structure
-   **gpio**: GPIO number to blink
-   **pd\_ms**: Blink period in milliseconds
-   **dr\_ms**: Blink duration in milliseconds (use `NBLK_FOREVER` for infinite)
-   **sl**: Stop level when blinking ends
-   **prio**: Priority level
-   **Returns**: `true` if successful, `false` if failed

#### `nblk_is_sync_blinking(nblk_mgr_t* mgr, gpio_num_t gpio)`

Check if a GPIO is blinking under synchronized control.

-   **mgr**: Pointer to manager structure
-   **gpio**: GPIO number to check
-   **Returns**: `true` if blinking, `false` if not

#### `nblk_sync_stop(nblk_mgr_t* mgr, gpio_num_t gpio, bool sl)`

Stop synchronized blinking on a GPIO.

-   **mgr**: Pointer to manager structure
-   **gpio**: GPIO number to stop
-   **sl**: Stop level to set
-   **Returns**: `true` if successful, `false` if failed

## Important Notes

1.  **Minimum Period**: The minimum blink period is `2*portTICK_PERIOD_MS`
2.  **Duration Validation**: For regular blinking, `dr_ms` must be a multiple of `pd_ms` (unless using `NBLK_FOREVER`)
3.  **Timebase Selection**: For synchronized blinking, choose a timebase that is the highest common factor of all blink periods for optimal performance
4.  **Memory Management**: The component handles dynamic memory allocation internally
5.  **Thread Safety**: All API functions are thread-safe and can be called from different tasks

## Error Handling

The component includes error checking for:

-   Invalid blink periods
-   Invalid duration values
-   Memory allocation failures
-   Timer creation failures

Check the return values of API functions and monitor ESP-IDF logs for error messages (tag: `"nblk"`).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
