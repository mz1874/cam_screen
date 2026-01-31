#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// 引入 BSP 头文件
#include "bsp/m5stack_tab5.h"
// 引入 LVGL 配置
#include "../lv_conf.h"
// 引入 LVGL (BSP 通常会自动包含 LVGL)
#include "lvgl.h"

static const char *TAG = "M5_TAB5_EXAMPLE";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting M5Stack Tab5 initialization...");

    /* 
     *1. 初始化 I2C 总线
     */
    bsp_i2c_init();

    /* 2. 初始化显示屏和 LVGL
     * 这会自动配置 SPI/8080 接口并启动 LVGL 定时器任务
     */
    bsp_display_start();

    /* 3. 使用 LVGL 绘制内容 (线程安全方式) */
    bsp_display_lock(0); // 获取显示锁

    // 获取当前活动屏幕
    lv_obj_t *scr = lv_scr_act();

    // 设置背景为白色 (确保你能看出来屏幕亮了)
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);

    // 创建一个标签
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello EEPW ! Hello M5Stack Tab5!");
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, LV_PART_MAIN); // 字体大小
    lv_obj_center(label);                                                    // 居中显示

    bsp_display_unlock(); // 释放显示锁

    //开启屏幕背光
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Display initialized and backlight on.");
}