#ifndef EMPTY_H
#define EMPTY_H

#include <stdint.h>
#include <stdbool.h> 

#define WIFI_SSID     "iPhone - Robert"
#define WIFI_PASSWORD "robita2004"

#define TIMEZONE 2

#define DEBOUNCE_DELAY  50000
#define SENSOR_INTERVAL 2000000

#define BTN_MODE_PIN   14
#define BTN_ACTION_PIN 15
#define BTN_SET_PIN    16

typedef enum
{
    SCREEN_HOME = 0,
    SCREEN_ALARM,
    SCREEN_STOPWATCH,
    SCREEN_COUNT
} ScreenMode;

typedef struct
{
    uint8_t pin;
    bool stable_state;
    bool last_raw_state;
    uint32_t last_change_us;
} Button;

bool button_pressed(Button *btn, uint32_t current_us);
void alarm_add_15_min(uint8_t *hour, uint8_t *min);

#endif