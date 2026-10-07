#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ssd1306.h"

static const char *TAG = "TAMAGOTCHI";

#define I2C_SDA_GPIO       21
#define I2C_SCL_GPIO       22
#define OLED_I2C_ADDRESS   0x3C

#define OLED_WIDTH         128
#define OLED_HEIGHT        64

static uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

/* =========================================================
 * PIXEL FUNCTIONS
 * ========================================================= */

static void set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH ||
        y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    int index = x + (y / 8) * OLED_WIDTH;
    uint8_t bit = 1 << (y % 8);

    if (on)
        oled_buffer[index] |= bit;
    else
        oled_buffer[index] &= ~bit;
}

static void fill_rect(int x1, int y1, int x2, int y2)
{
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            set_pixel(x, y, true);
        }
    }
}

static void draw_rect(int x1, int y1, int x2, int y2)
{
    for (int x = x1; x <= x2; x++) {
        set_pixel(x, y1, true);
        set_pixel(x, y2, true);
    }

    for (int y = y1; y <= y2; y++) {
        set_pixel(x1, y, true);
        set_pixel(x2, y, true);
    }
}

/* =========================================================
 * GROUND
 * ========================================================= */

static void draw_ground(int offset)
{
    fill_rect(0, 54, 127, 55);

    /* Moving ground marks */
    for (int x = -offset; x < 128; x += 16) {
        fill_rect(x, 58, x + 6, 59);
    }
}

/* =========================================================
 * PET / DINO
 * ========================================================= */

static void draw_pet(int x, int y, bool frame)
{
    /*
     * Body
     */
    fill_rect(x + 4, y + 8, x + 25, y + 24);

    /*
     * Head
     */
    fill_rect(x + 15, y + 3, x + 29, y + 15);

    /*
     * Ear
     */
    fill_rect(x + 18, y, x + 22, y + 4);

    /*
     * Eye
     */
    set_pixel(x + 25, y + 6, false);

    /*
     * Nose
     */
    fill_rect(x + 29, y + 10, x + 31, y + 12);

    /*
     * Tail
     */
    fill_rect(x, y + 10, x + 5, y + 15);
    fill_rect(x - 3, y + 7, x + 1, y + 11);

    /*
     * Legs
     *
     * Alternate frames to create running animation.
     */
    if (frame) {
        fill_rect(x + 7, y + 24, x + 11, y + 31);
        fill_rect(x + 19, y + 24, x + 23, y + 28);
    } else {
        fill_rect(x + 7, y + 24, x + 11, y + 28);
        fill_rect(x + 19, y + 24, x + 23, y + 31);
    }
}

/* =========================================================
 * CACTUS
 * ========================================================= */

static void draw_cactus(int x)
{
    /*
     * Main trunk
     */
    fill_rect(x + 4, 38, x + 8, 54);

    /*
     * Left branch
     */
    fill_rect(x, 42, x + 4, 46);
    fill_rect(x, 39, x + 3, 43);

    /*
     * Right branch
     */
    fill_rect(x + 8, 44, x + 12, 48);
    fill_rect(x + 10, 40, x + 12, 45);
}

/* =========================================================
 * SCORE
 * ========================================================= */

static void draw_digit(int x, int y, int digit)
{
    /*
     * 3x5 pixel digits
     */
    static const uint8_t digits[10][5] = {
        {7,5,5,5,7},
        {2,6,2,2,7},
        {7,1,7,4,7},
        {7,1,7,1,7},
        {5,5,7,1,1},
        {7,4,7,1,7},
        {7,4,7,5,7},
        {7,1,1,1,1},
        {7,5,7,5,7},
        {7,5,7,1,7}
    };

    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (digits[digit][row] & (1 << (2 - col))) {
                fill_rect(
                    x + col * 2,
                    y + row * 2,
                    x + col * 2 + 1,
                    y + row * 2 + 1
                );
            }
        }
    }
}

static void draw_score(int score)
{
    int hundreds = (score / 100) % 10;
    int tens = (score / 10) % 10;
    int ones = score % 10;

    /* SCORE label using simple blocks */
    fill_rect(2, 2, 3, 8);
    fill_rect(4, 2, 7, 3);
    fill_rect(4, 5, 6, 6);
    fill_rect(4, 8, 7, 9);

    /* Three digits */
    draw_digit(12, 2, hundreds);
    draw_digit(20, 2, tens);
    draw_digit(28, 2, ones);
}

/* =========================================================
 * GAME OVER
 * ========================================================= */

static void draw_game_over(void)
{
    memset(oled_buffer, 0, sizeof(oled_buffer));

    /*
     * GAME OVER represented with large block letters.
     */

    /* G */
    fill_rect(20, 22, 24, 40);
    fill_rect(24, 22, 31, 26);
    fill_rect(24, 38, 31, 42);
    fill_rect(28, 31, 31, 35);

    /* A */
    fill_rect(35, 22, 39, 42);
    fill_rect(43, 22, 47, 42);
    fill_rect(39, 22, 43, 26);
    fill_rect(39, 31, 43, 35);

    /* M */
    fill_rect(51, 22, 55, 42);
    fill_rect(63, 22, 67, 42);
    fill_rect(55, 22, 59, 30);
    fill_rect(59, 26, 63, 30);

    /* E */
    fill_rect(71, 22, 75, 42);
    fill_rect(75, 22, 82, 26);
    fill_rect(75, 31, 81, 35);
    fill_rect(75, 38, 82, 42);

    /*
     * Small "OVER" indicator
     */
    fill_rect(43, 48, 85, 50);
}

/* =========================================================
 * OLED UPDATE
 * ========================================================= */

static void update_display(esp_lcd_panel_handle_t panel_handle)
{
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap(
            panel_handle,
            0,
            0,
            OLED_WIDTH,
            OLED_HEIGHT,
            oled_buffer
        )
    );
}

/* =========================================================
 * MAIN
 * ========================================================= */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Tamagotchi Dino Game...");

    /* -----------------------------------------------------
     * I2C BUS
     * ----------------------------------------------------- */

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle = NULL;

    ESP_ERROR_CHECK(
        i2c_new_master_bus(
            &bus_config,
            &bus_handle
        )
    );

    /* -----------------------------------------------------
     * OLED I2C
     * ----------------------------------------------------- */

    esp_lcd_panel_io_handle_t io_handle = NULL;

    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = OLED_I2C_ADDRESS,
        .scl_speed_hz = 400000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
    };

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_i2c(
            bus_handle,
            &io_config,
            &io_handle
        )
    );

    /* -----------------------------------------------------
     * SSD1306
     * ----------------------------------------------------- */

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = OLED_HEIGHT,
        .contrast = 128,
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
        .vendor_config = &ssd1306_config,
    };

    esp_lcd_panel_handle_t panel_handle = NULL;

    ESP_ERROR_CHECK(
        esp_lcd_new_panel_ssd1306(
            io_handle,
            &panel_config,
            &panel_handle
        )
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_reset(panel_handle)
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_init(panel_handle)
    );

    ESP_ERROR_CHECK(
        esp_lcd_panel_disp_on_off(
            panel_handle,
            true
        )
    );

    ESP_LOGI(TAG, "Dino game started!");

    /* =====================================================
     * GAME VARIABLES
     * ===================================================== */

    int pet_x = 18;
    int pet_y = 23;

    int obstacle_x = 120;

    int jump_height = 0;
    int jump_velocity = 0;

    bool jumping = false;
    bool game_over = false;

    int score = 0;

    int ground_offset = 0;

    bool run_frame = false;

    /* =====================================================
     * GAME LOOP
     * ===================================================== */

    while (1) {

        /* -------------------------------------------------
         * GAME OVER
         * ------------------------------------------------- */

        if (game_over) {

            draw_game_over();
            update_display(panel_handle);

            vTaskDelay(pdMS_TO_TICKS(2000));

            /*
             * Restart
             */
            obstacle_x = 120;
            jump_height = 0;
            jump_velocity = 0;
            jumping = false;
            score = 0;
            ground_offset = 0;
            game_over = false;

            continue;
        }

        /* -------------------------------------------------
         * AUTOMATIC JUMP
         *
         * When obstacle gets close, jump automatically.
         * ------------------------------------------------- */

        if (!jumping && obstacle_x < 65 && obstacle_x > 25) {
            jumping = true;
            jump_velocity = 9;
        }

        /* -------------------------------------------------
         * JUMP PHYSICS
         * ------------------------------------------------- */

        if (jumping) {

            jump_height += jump_velocity;
            jump_velocity -= 1;

            if (jump_height <= 0) {
                jump_height = 0;
                jump_velocity = 0;
                jumping = false;
            }
        }

        /* -------------------------------------------------
         * MOVE OBSTACLE
         * ------------------------------------------------- */

        obstacle_x -= 3;

        if (obstacle_x < -15) {
            obstacle_x = 128;

            score++;

            ESP_LOGI(
                TAG,
                "Score: %d",
                score
            );
        }

        /* -------------------------------------------------
         * RUNNING ANIMATION
         * ------------------------------------------------- */

        run_frame = !run_frame;

        /* -------------------------------------------------
         * GROUND ANIMATION
         * ------------------------------------------------- */

        ground_offset += 3;

        if (ground_offset >= 16) {
            ground_offset = 0;
        }

        /* -------------------------------------------------
         * COLLISION DETECTION
         * ------------------------------------------------- */

        int pet_bottom = 54 - jump_height;

        /*
         * Horizontal collision
         */
        bool horizontal_collision =
            (pet_x + 29 >= obstacle_x) &&
            (pet_x <= obstacle_x + 12);

        /*
         * Vertical collision
         */
        bool vertical_collision =
            (pet_bottom >= 38);

        if (horizontal_collision && vertical_collision) {

            ESP_LOGI(
                TAG,
                "GAME OVER! Score: %d",
                score
            );

            game_over = true;

            continue;
        }

        /* -------------------------------------------------
         * DRAW SCREEN
         * ------------------------------------------------- */

        memset(
            oled_buffer,
            0,
            sizeof(oled_buffer)
        );

        draw_score(score);

        draw_ground(ground_offset);

        draw_pet(
            pet_x,
            23 - jump_height,
            run_frame
        );

        draw_cactus(obstacle_x);

        update_display(panel_handle);

        /* -------------------------------------------------
         * GAME SPEED
         * ------------------------------------------------- */

        vTaskDelay(
            pdMS_TO_TICKS(90)
        );
    }
}