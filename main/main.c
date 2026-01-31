#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <string.h>
#include "bsp/m5stack_tab5.h"
#include "../lv_conf.h"
#include "lvgl.h"
#include "main_functions.h"

static const char *TAG = "M5_TAB5_EXAMPLE";

// 从较小的网格开始，逐步增加
#define GRID_SIZE 28
static lv_obj_t **grid_objects = NULL; // 使用一维数组简化管理
static uint32_t grid_count = 0;
static lv_obj_t *title = NULL; // 标题标签，用于显示推理结果

// 内存监控
static void memory_monitor(void)
{
    static uint32_t last_check = 0;
    uint32_t now = esp_log_timestamp();

    if (now - last_check > 5000)
    { // 每5秒检查一次
        last_check = now;

        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t largest_psram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        ESP_LOGI(TAG, "Memory - PSRAM: free=%dKB, largest=%dKB, Internal: free=%dKB",
                 free_psram / 1024, largest_psram / 1024, free_internal / 1024);
    }
}

static void grid_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_color_t cur = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    if (lv_color_eq(cur, lv_color_white()))
    {
        lv_obj_set_style_bg_color(obj, lv_color_black(), LV_PART_MAIN);
    }
    else
    {
        lv_obj_set_style_bg_color(obj, lv_color_white(), LV_PART_MAIN);
    }
}

static void infer_cb(lv_event_t *e)
{
    // 收集屏幕网格的像素数据
    uint8_t img[28 * 28] = {0};
    for (int i = 0; i < 28 * 28; i++)
    {
        if (grid_objects && grid_objects[i])
        {
            lv_color_t color = lv_obj_get_style_bg_color(grid_objects[i], LV_PART_MAIN);
            // 黑色像素设为255，白色设为0
            img[i] = lv_color_eq(color, lv_color_black()) ? 255 : 0;
        }
    }

    // 进行推理
    char result[50];
    mnist_infer(img, result, sizeof(result));

    // 更新标题显示推理结果
    if (title)
    {
        lv_label_set_text(title, result);
    }
}

static void clear_grid_cb(lv_event_t *e)
{
    for (uint32_t i = 0; i < GRID_SIZE * GRID_SIZE; i++)
    {
        if (grid_objects && grid_objects[i])
            lv_obj_set_style_bg_color(grid_objects[i], lv_color_white(), LV_PART_MAIN);
    }
}

// 创建网格的辅助函数（支持偏移，便于居中）
static bool create_grid_cell(lv_obj_t *scr, int row, int col,
                             lv_coord_t cell_width, lv_coord_t cell_height,
                             lv_coord_t offset_x, lv_coord_t offset_y)
{
    lv_obj_t *obj = lv_obj_create(scr);
    if (!obj)
    {
        ESP_LOGE(TAG, "Failed to create cell [%d][%d]", row, col);
        return false;
    }

    // 存储对象指针（可选）
    if (grid_objects)
    {
        grid_objects[grid_count++] = obj;
    }

    lv_obj_set_size(obj, cell_width, cell_height);
    lv_obj_set_pos(obj, offset_x + col * cell_width, offset_y + row * cell_height);

    // 简化样式
    lv_obj_set_style_bg_color(obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);

    // 移除内边距和外边距
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(obj, 0, LV_PART_MAIN);

    // 添加点击事件
    lv_obj_add_event_cb(obj, grid_event_cb, LV_EVENT_CLICKED, NULL);

    return true;
}

// 分步创建网格
static bool create_grid_incremental(lv_obj_t *scr, lv_coord_t offset_x, lv_coord_t offset_y, lv_coord_t cell_size)
{
    ESP_LOGI(TAG, "Creating %dx%d grid incrementally...", GRID_SIZE, GRID_SIZE);

    ESP_LOGI(TAG, "Cell size: %dx%d", cell_size, cell_size);

    // 计算需要的内存
    size_t total_cells = GRID_SIZE * GRID_SIZE;
    ESP_LOGI(TAG, "Total cells: %d", total_cells);

    // 分配对象指针数组（使用PSRAM）
    grid_objects = (lv_obj_t **)heap_caps_malloc(total_cells * sizeof(lv_obj_t *),
                                                 MALLOC_CAP_SPIRAM);
    if (!grid_objects)
    {
        ESP_LOGW(TAG, "Failed to allocate grid objects array in PSRAM, using internal");
        grid_objects = (lv_obj_t **)malloc(total_cells * sizeof(lv_obj_t *));
        if (!grid_objects)
        {
            ESP_LOGE(TAG, "Failed to allocate grid objects array");
            return false;
        }
    }
    memset(grid_objects, 0, total_cells * sizeof(lv_obj_t *));

    // 分批创建网格，避免一次性创建太多对象
    int batch_size = 50; // 每批创建50个对象
    int created = 0;

    for (int i = 0; i < GRID_SIZE; i++)
    {
        for (int j = 0; j < GRID_SIZE; j++)
        {
            if (!create_grid_cell(scr, i, j, cell_size, cell_size, offset_x, offset_y))
            {
                ESP_LOGE(TAG, "Failed at cell [%d][%d]", i, j);
                continue;
            }
            created++;
            if (created % batch_size == 0)
            {
                ESP_LOGI(TAG, "Created %d/%d cells", created, total_cells);
                lv_timer_handler();
                vTaskDelay(pdMS_TO_TICKS(10));
                memory_monitor();
            }
        }
        ESP_LOGI(TAG, "Completed row %d/%d", i + 1, GRID_SIZE);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGI(TAG, "Grid creation completed: %d cells created", created);
    return created > 0;
}

void inference()
{
    while (1)
    {
        loop();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void app_main(void)
{
    setup();
    ESP_LOGI(TAG, "Starting M5Stack Tab5 Grid Demo");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    xTaskCreatePinnedToCore(inference, "inference_task", 8 * 1024, NULL, 5, NULL, 1);
    // 显示内存信息
    ESP_LOGI(TAG, "Initial PSRAM free: %dKB",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    ESP_LOGI(TAG, "Initial Internal free: %dKB",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);

    // 1. 初始化硬件
    bsp_i2c_init();
    bsp_display_start();
    bsp_display_backlight_on();

    // 2. 获取屏幕并设置背景
    bsp_display_lock(0);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), LV_PART_MAIN);

    // 3. 先创建一个标题标签
    title = lv_label_create(scr);
    lv_label_set_text(title, "Digital recognization from EEPW");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 4. 创建信息标签
    lv_obj_t *info = lv_label_create(scr);
    lv_label_set_text(info, "Creating grid...");
    lv_obj_set_style_text_color(info, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 50);

    bsp_display_unlock();

    // 给用户一点时间看到提示
    for (int i = 0; i < 10; i++)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 5. 创建正方形网格（居中）
    bsp_display_lock(0);

    // 计算正方形区域
    lv_coord_t screen_width = lv_obj_get_width(scr);
    lv_coord_t screen_height = lv_obj_get_height(scr);
    lv_coord_t grid_size_px = screen_width < screen_height ? screen_width : screen_height;
    lv_coord_t cell_size = grid_size_px / GRID_SIZE;
    lv_coord_t offset_x = (screen_width - grid_size_px) / 2;
    lv_coord_t offset_y = (screen_height - grid_size_px) / 2 + 40; // 下移避开标题

    // 更新信息标签
    lv_label_set_text(info, "Creating grid cells...");
    lv_timer_handler();

    bool success = create_grid_incremental(scr, offset_x, offset_y, cell_size);

    if (success)
    {
        lv_label_set_text(info, "Grid ready! Touch cells to color");
        ESP_LOGI(TAG, "=== Grid Demo Ready ===");
        ESP_LOGI(TAG, "Touch any cell to change its color");
    }
    else
    {
        lv_label_set_text(info, "Grid creation failed");
        ESP_LOGE(TAG, "Grid creation failed");
    }

    // 创建 Clear 按钮
    lv_obj_t *btn_clear = lv_btn_create(scr);
    lv_obj_set_size(btn_clear, 100, 40);
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *label_clear = lv_label_create(btn_clear);
    lv_label_set_text(label_clear, "Clear");
    lv_obj_center(label_clear);
    lv_obj_add_event_cb(btn_clear, clear_grid_cb, LV_EVENT_CLICKED, NULL);

    // 创建 Inference 按钮
    lv_obj_t *btn_infer = lv_btn_create(scr);
    lv_obj_set_size(btn_infer, 100, 40);
    lv_obj_align(btn_infer, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_t *label_infer = lv_label_create(btn_infer);
    lv_label_set_text(label_infer, "Inference");
    lv_obj_center(label_infer);
    lv_obj_add_event_cb(btn_infer, infer_cb, LV_EVENT_CLICKED, NULL);

    bsp_display_unlock();

    // 6. 主循环
    uint32_t last_mem_check = 0;
    while (1)
    {
        uint32_t now = esp_log_timestamp();

        // 处理LVGL事件
        lv_timer_handler();

        // 定期检查内存
        if (now - last_mem_check > 10000)
        { // 每10秒
            last_mem_check = now;
            memory_monitor();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // 清理内存（实际上不会执行到这里）
    if (grid_objects)
    {
        heap_caps_free(grid_objects);
        grid_objects = NULL;
    }
}