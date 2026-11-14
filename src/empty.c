#include <stdio.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/aon_timer.h"
#include "hardware/i2c.h"
#include "lwip/apps/sntp.h"

#include "ssd1306.h"
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

        //Citire SENZORI (ex. DHT11 / Presiune la fiecare 2 secunde)
        if ((current_us - last_sensor_time_us) >= SENSOR_INTERVAL) {
            last_sensor_time_us = current_us;

            // Aici vei apela functiile tale de citire pentru senzori:
            // dht11_read_reg(&temp, &hum);
            // pressure = read_pressure();

            force_redraw = true; // Actualizam valorile pe ecran dupa citire
        }

        aon_timer_get_time_calendar(&now);

        if (now.tm_sec != last_sec || force_redraw) {
            last_sec = now.tm_sec;
            force_redraw = false;

            memset(buf, 0, SSD1306_BUF_LEN);
            WriteString(buf, 28, 0, "CONNECTED");
            WriteString(buf, 120, 0, "!");
            
            WriteString(buf, 0, 48, "HUM   TEMP   BAR");
            WriteString(buf, 0, 56, "--%   ---*  ---K");

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
