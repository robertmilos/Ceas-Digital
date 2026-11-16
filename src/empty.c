#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/aon_timer.h"
#include "hardware/i2c.h"
#include "lwip/apps/sntp.h"

#include "ssd1306.h"
#include "bmp280.h"
#include "dht20.h"
#include "empty.h"

static volatile bool time_sync = false;

void set_system_time_us(uint32_t sec, uint32_t us)
{
    time_t local_sec = (time_t)sec + (TIMEZONE * 3600);
    struct timespec ts =
    {
        .tv_sec = local_sec,
        .tv_nsec = (long)us * 1000
    };
    aon_timer_set_time(&ts);
    time_sync = true;
}

int main()
{
    stdio_init_all();

    gpio_init(BTN_MODE_PIN);
    gpio_set_dir(BTN_MODE_PIN, GPIO_IN);

    gpio_init(BTN_ACTION_PIN);
    gpio_set_dir(BTN_ACTION_PIN, GPIO_IN);

    gpio_init(BTN_SET_PIN);
    gpio_set_dir(BTN_SET_PIN, GPIO_IN);

    i2c_init(i2c0, SSD1306_I2C_CLK * 1000);
    gpio_set_function(20, GPIO_FUNC_I2C);
    gpio_set_function(21, GPIO_FUNC_I2C);

    sleep_ms(100);

    SSD1306_init();
    struct timespec initial_ts = {0};
    aon_timer_start(&initial_ts);

    struct render_area frame_area =
    {
        .start_col = 0,
        .end_col = SSD1306_WIDTH - 1,
        .start_page = 0,
        .end_page = SSD1306_NUM_PAGES - 1
    };
    calc_render_area_buflen(&frame_area);

    uint8_t buf[SSD1306_BUF_LEN];

    memset(buf, 0, SSD1306_BUF_LEN);
    WriteString(buf, 24, 0, "CONNECTING...");
    WriteString(buf, 80, 16, "-----");
    WriteString(buf, 0, 24, "24H");
    WriteString2x(buf, 24, 24, "--:--");
    WriteString(buf, 104, 32, "--");
    WriteString(buf, 0, 48, "HUM   TEMP   BRM");
    WriteString(buf, 0, 56, "--%   ---*  ---K");
    render(buf, &frame_area);

    if (cyw43_arch_init()) {
        memset(buf, 0, SSD1306_WIDTH);
        WriteString(buf, 12, 0, "NOT CONNECTED");
        render(buf, &frame_area);
        while (true) tight_loop_contents();
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        memset(buf, 0, SSD1306_WIDTH);
        WriteString(buf, 12, 0, "NOT CONNECTED");
        render(buf, &frame_area);
        while (true) tight_loop_contents();
    }

    i2c_init(i2c1, 100 * 1000); 
    gpio_set_function(18, GPIO_FUNC_I2C);
    gpio_set_function(19, GPIO_FUNC_I2C);

    sleep_ms(100);

    bmp280_init();

    struct bmp280_calib_param bmp_params;
    bmp280_get_calib_params(&bmp_params);

    DHT20 dht_sensor;
    DHT20_init(&dht_sensor);
    float current_hum_pct = 0.0f;

    int32_t raw_temperature = 0;
    int32_t raw_pressure = 0;
    float current_temp_c = 0.0f;
    float current_press_kpa = 0.0f;

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    while (!time_sync) {
        cyw43_arch_poll();
        tight_loop_contents();
    }

    char date_str[6];
    char *months[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP","OCT", "NOV", "DEC"};
    char time_str[6];
    char sec_str[3];
    struct tm now;

    int last_sec = -1;
    bool is_12h_format = false;
    bool force_redraw = true;

    uint32_t sensor_last_time_us = 0;
    
    ScreenMode current_screen = SCREEN_HOME;
    Button btn_mode   = { .pin = BTN_MODE_PIN,   .stable_state = true, .last_raw_state = true, .last_change_us = 0 };
    Button btn_action = { .pin = BTN_ACTION_PIN, .stable_state = true, .last_raw_state = true, .last_change_us = 0 };
    Button btn_set = { .pin = BTN_SET_PIN, .stable_state = true, .last_raw_state = true, .last_change_us = 0 };

    uint8_t alarm_hour = 0;
    uint8_t alarm_min = 0;
    bool alarm_enabled = false;
    uint32_t set_hold_start_us = 0;
    uint32_t set_last_repeat_us = 0;
    bool set_is_holding = false;
    uint32_t action_hold_start_us = 0;
    bool action_is_holding = false;

    bool sw_running = false;
    uint32_t sw_start_us = 0;
    uint32_t sw_elapsed_ms = 0;
    uint32_t last_sw_draw_ms = 0;

    while (true) {

        cyw43_arch_poll();

        uint32_t current_us = time_us_32();

        if (button_pressed(&btn_mode, current_us)) 
        {
            current_screen = (current_screen + 1) % SCREEN_COUNT;
            force_redraw = true;
        }

        if (button_pressed(&btn_action, current_us)) 
        {
            switch (current_screen) 
            {
                case SCREEN_HOME:
                    is_12h_format = !is_12h_format;
                    break;

                case SCREEN_ALARM:
                    alarm_enabled = !alarm_enabled;
                    action_hold_start_us = current_us;
                    action_is_holding = true;
                    break;

                case SCREEN_STOPWATCH:
                    if (!sw_running) 
                    {
                        sw_start_us = current_us - (sw_elapsed_ms * 1000);
                        sw_running = true;
                    } 
                    else 
                    {
                        sw_elapsed_ms = (current_us - sw_start_us) / 1000;
                        sw_running = false;
                    }
                    break;

                default:
                    break;
            }
            force_redraw = true;
        }

        if (button_pressed(&btn_set, current_us)) 
        {
            switch (current_screen) 
            {
                case SCREEN_ALARM:
                {
                    alarm_add_15_min(&alarm_hour, &alarm_min);
                    set_hold_start_us = current_us;
                    set_last_repeat_us = current_us;
                    set_is_holding = true;
                    force_redraw = true;
                    break;
                }

                case SCREEN_STOPWATCH:
                {
                    sw_running = false;
                    sw_elapsed_ms = 0;
                    sw_start_us = 0;
                    last_sw_draw_ms = 0;
                    force_redraw = true;
                    break;
                }

                default:
                    break;
            }
        }

        if (current_screen == SCREEN_ALARM && action_is_holding) 
        {
            if (btn_action.stable_state == false) 
            {
                if ((current_us - action_hold_start_us) >= 1000000) 
                {
                    alarm_hour = 0;
                    alarm_min = 0;
                    alarm_enabled = false;
                    action_is_holding = false;
                    force_redraw = true;
                }
            } 
            else 
            {
                action_is_holding = false;
            }
        }

        if (current_screen == SCREEN_ALARM && set_is_holding) 
        {
            if (btn_set.stable_state == false) 
            {
                if ((current_us - set_hold_start_us) >= 1000000 &&
                    (current_us - set_last_repeat_us) >= 100000) 
                {
                    set_last_repeat_us = current_us;
                    alarm_add_15_min(&alarm_hour, &alarm_min);
                    force_redraw = true;
                }
            } 
            else 
            {
                set_is_holding = false;
            }
        }

        if (sw_running) 
        {
            sw_elapsed_ms = (current_us - sw_start_us) / 1000;
            if (current_screen == SCREEN_STOPWATCH && (sw_elapsed_ms - last_sw_draw_ms) >= 100) 
            {
                last_sw_draw_ms = sw_elapsed_ms;
                force_redraw = true;
            }
        }

        if ((current_us - sensor_last_time_us) >= SENSOR_INTERVAL) {
            sensor_last_time_us = current_us;

            bmp280_read_raw(&raw_temperature, &raw_pressure);
            
            int32_t temp_cal = bmp280_convert_temp(raw_temperature, &bmp_params);
            int32_t press_cal = bmp280_convert_pressure(raw_pressure, raw_temperature, &bmp_params);
            
            current_temp_c = temp_cal / 100.0f;
            current_press_kpa = press_cal / 1000.0f;

            if (getMeasurement(&dht_sensor) == DHT20_OK) {
                current_hum_pct = getHumidity(&dht_sensor);
            }

            force_redraw = true; 
        }

        aon_timer_get_time_calendar(&now);

        if (now.tm_sec != last_sec || force_redraw) {
            last_sec = now.tm_sec;
            force_redraw = false;

            memset(buf, 0, SSD1306_BUF_LEN);

            switch (current_screen)
            {
                case SCREEN_HOME:
                {
                    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
                    if (link_status == CYW43_LINK_UP)
                    {
                        WriteString(buf, 28, 0, "CONNECTED");
                        WriteString(buf, 120, 0, "!");
                    }
                    else
                    {
                        WriteString(buf, 12, 0, "NOT CONNECTED");
                    }

                    if (alarm_enabled)
                    {
                        WriteString(buf, 0, 0, "@");
                    }
                    else
                    {
                        WriteString(buf, 0, 0, " ");
                    }
            
                    WriteString(buf, 0, 48, "HUM   TEMP   BRM");
                    char sensor_str[24];
                    snprintf(sensor_str, sizeof(sensor_str), "%2.0f%%    %2.0f*  %3.0fk", current_hum_pct, current_temp_c, current_press_kpa);
                    WriteString(buf, 0, 56, sensor_str);

                    if (is_12h_format) 
                    {
                        int hour_12 = now.tm_hour % 12;
                        if (hour_12 == 0) hour_12 = 12;
                        char *period = (now.tm_hour >= 12) ? "PM" : "AM";

                        snprintf(time_str, sizeof(time_str), "%2d:%02d", hour_12, now.tm_min);
                        WriteString(buf, 0, 24, period);
                        WriteString2x(buf, 24, 24, time_str);
                    } 
                    else 
                    {
                        snprintf(time_str, sizeof(time_str), "%2d:%02d", now.tm_hour, now.tm_min);
                        WriteString(buf, 0, 24, "24H");
                        WriteString2x(buf, 24, 24, time_str);
                    }

                    snprintf(sec_str, sizeof(sec_str), "%02d", now.tm_sec);
                    WriteString(buf, 104, 32, sec_str);

                    snprintf(date_str, sizeof(date_str), "%3s%2d", months[now.tm_mon], now.tm_mday);
                    WriteString(buf, 80, 16, date_str);
                    break;
                }

                case SCREEN_ALARM:
                {
                    WriteString(buf, 44, 0, "ALARM");
                    WriteString(buf, 0, 0, alarm_enabled ? "@" : " ");
                    if (is_12h_format) 
                    {
                        int hour_12 = alarm_hour % 12;
                        if (hour_12 == 0) hour_12 = 12;
                        char *period = (alarm_hour >= 12) ? "PM" : "AM";

                        snprintf(time_str, sizeof(time_str), "%2d:%02d", hour_12, alarm_min);
                        WriteString(buf, 0, 24, period);
                        WriteString2x(buf, 24, 24, time_str);
                    } 
                    else 
                    {
                        snprintf(time_str, sizeof(time_str), "%2d:%02d", alarm_hour, alarm_min);
                        WriteString(buf, 0, 24, "24H");
                        WriteString2x(buf, 24, 24, time_str);
                    }
                    break;
                }

                case SCREEN_STOPWATCH: 
                {
                    WriteString(buf, 28, 0, "STOPWATCH");
                    uint32_t total_sec = sw_elapsed_ms / 1000;
                    uint32_t mins = total_sec / 60;
                    uint32_t secs = total_sec % 60;
                    uint32_t tenths = (sw_elapsed_ms % 1000) / 100;

                    snprintf(time_str, sizeof(time_str), "%02lu:%02lu", mins, secs);
                    WriteString2x(buf, 24, 24, time_str);

                    snprintf(sec_str, sizeof(sec_str), ".%lu", tenths);
                    WriteString(buf, 104, 32, sec_str);

                    break;
                }


                default:
                    break;
            }
            render(buf, &frame_area);
        }
    }

    return 0;
}
