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

    i2c_init(i2c0, SSD1306_I2C_CLK * 1000);
    gpio_set_function(16, GPIO_FUNC_I2C);
    gpio_set_function(17, GPIO_FUNC_I2C);
    gpio_pull_up(16);
    gpio_pull_up(17);

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
    WriteString(buf, 0, 48, "HUM   TEMP   BAR");
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

    memset(buf, 0, SSD1306_WIDTH);
    WriteString(buf, 24, 0, "CONNECTING...");
    render(buf, &frame_area);

    i2c_init(i2c1, 100 * 1000); 
    gpio_set_function(18, GPIO_FUNC_I2C);
    gpio_set_function(19, GPIO_FUNC_I2C);
    gpio_pull_up(18);
    gpio_pull_up(19);

    sleep_ms(100);

    bmp280_init();

    struct bmp280_calib_param bmp_params;
    bmp280_get_calib_params(&bmp_params);

    DHT20 dht_sensor;
    int dht_status = DHT20_init(&dht_sensor);
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
    char fmt_str[4];
    char sec_str[3];
    struct tm now;

    int last_sec = -1;
    bool is_12h_format = false;
    bool force_redraw = true;

    bool last_btn_state = true;
    uint32_t last_btn_time_us = 0;
    uint32_t last_sensor_time_us;
    

    while (true) {

        cyw43_arch_poll();

        uint32_t current_us = time_us_32();

        // bool current_btn_state = gpio_get(BTN1);
        // if (last_btn_state && !current_btn_state) {
            // if ((current_us - last_btn_time_us) > DEBOUNCE_DELAY) {
                // last_btn_time_us = current_us;
                // is_12h_format = !is_12h_format;
                // force_redraw = true;
            // }
        // }
        // last_btn_state = current_btn_state;

        //Citire SENZORI 
        if ((current_us - last_sensor_time_us) >= SENSOR_INTERVAL) {
            last_sensor_time_us = current_us;

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
            WriteString(buf, 28, 0, "CONNECTED");
            WriteString(buf, 120, 0, "!");
            
            WriteString(buf, 0, 48, "HUM   TEMP   BAR");
            char sensor_str[24];
            snprintf(sensor_str, sizeof(sensor_str), "%2.0f%%    %2.0f*  %3.0fK", current_hum_pct, current_temp_c, current_press_kpa);
            
            WriteString(buf, 0, 56, sensor_str);

            if (is_12h_format) {
                int hour_12 = now.tm_hour % 12;
                if (hour_12 == 0) hour_12 = 12;
                char *period = (now.tm_hour >= 12) ? "PM" : "AM";

                snprintf(time_str, sizeof(time_str), "%2d:%02d", hour_12, now.tm_min);
                WriteString(buf, 0, 24, period);
                WriteString2x(buf, 24, 24, time_str);
            } else {
                snprintf(time_str, sizeof(time_str), "%2d:%02d", now.tm_hour, now.tm_min);
                WriteString(buf, 0, 24, "24H");
                WriteString2x(buf, 24, 24, time_str);
            }

            snprintf(sec_str, sizeof(sec_str), "%02d", now.tm_sec);
            WriteString(buf, 104, 32, sec_str);

            snprintf(date_str, sizeof(date_str), "%3s%2d", months[now.tm_mon], now.tm_mday);
            WriteString(buf, 80, 16, date_str);

            render(buf, &frame_area);
        }
    }

    return 0;
}
