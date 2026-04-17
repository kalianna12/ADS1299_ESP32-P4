/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SSVEPApp.hpp"
#include <cmath>
#include <cstring>
#include "esp_log.h"
#include "esp_lv_adapter.h"

static const char *TAG = "SSVEPApp";

// 频率配置表
static constexpr const uint16_t FREQ_HZ[SSVEP_FREQ_NUM] = {8, 10, 12, 16};
static constexpr const uint16_t FREQ_PERIOD_MS[SSVEP_FREQ_NUM] = {125, 100, 83, 62};  // 1000/freq，单位ms

// 灰度查表大小（128个采样点，覆盖一个完整周期）
static constexpr const uint16_t GRAYSCALE_TABLE_SIZE = 128;

SSVEPApp::SSVEPApp():
    ESP_Brookesia_PhoneApp("SSVEP", nullptr, true),
    _app_area(nullptr),
    _update_task(nullptr),
    _task_running(false),
    _is_paused(false),
    _app_width(920),
    _app_height(470),
    _rect_size(300),
    _feedback_freq((ssvep_freq_t)(-1)),
    _feedback_start_time(0)
{
    std::memset(_freq_tables, 0, sizeof(_freq_tables));
    std::memset(_rects, 0, sizeof(_rects));
    std::memset(_feedback_rects, 0, sizeof(_feedback_rects));
}

SSVEPApp::~SSVEPApp()
{
    // 清理灰度表
    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        if (_freq_tables[i].grayscale_table != nullptr) {
            free(_freq_tables[i].grayscale_table);
            _freq_tables[i].grayscale_table = nullptr;
        }
    }
}

bool SSVEPApp::init(void)
{
    ESP_LOGI(TAG, "SSVEP App initializing...");
    
    // 初始化灰度查表
    initGrayscaleTables();
    
    return true;
}

bool SSVEPApp::run(void)
{
    ESP_LOGI(TAG, "SSVEP App running...");
    
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建应用显示区域
    _app_area = lv_obj_create(screen);
    lv_obj_set_size(_app_area, _app_width, _app_height);
    lv_obj_center(_app_area);
    lv_obj_set_style_radius(_app_area, 0, 0);
    lv_obj_set_style_border_width(_app_area, 1, 0);
    lv_obj_set_style_border_color(_app_area, lv_color_hex(0x353a46), 0);
    lv_obj_set_style_bg_color(_app_area, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_pad_all(_app_area, 0, 0);
    lv_obj_clear_flag(_app_area, LV_OBJ_FLAG_SCROLLABLE);
    
    // 计算方块大小（约占宽度的1/3）
    _rect_size = (_app_width / 3) - 20;
    
    // 创建四个角的闪烁方框
    createRectangles(_app_area);
    
    // 创建回显区域
    createFeedbackArea(_app_area);
    
    // 启动更新任务
    _task_running = true;
    xTaskCreatePinnedToCore(
        updateTaskEntry,
        "ssvep_update",
        4096,
        this,
        3,  // 优先级
        &_update_task,
        1   // 核心1
    );
    
    ESP_LOGI(TAG, "SSVEP App UI created successfully");
    return true;
}

bool SSVEPApp::back(void)
{
    ESP_LOGI(TAG, "SSVEP App back");
    return notifyCoreClosed();
}

bool SSVEPApp::close(void)
{
    ESP_LOGI(TAG, "SSVEP App closing");
    
    // 停止更新任务
    _task_running = false;
    
    // 等待任务完成
    if (_update_task != nullptr) {
        for (int i = 0; i < 100; i++) {
            if (eTaskGetState(_update_task) == eDeleted) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        _update_task = nullptr;
    }
    
    return true;
}

bool SSVEPApp::pause(void)
{
    _is_paused = true;
    return true;
}

bool SSVEPApp::resume(void)
{
    _is_paused = false;
    return true;
}

void SSVEPApp::initGrayscaleTables(void)
{
    ESP_LOGI(TAG, "Initializing grayscale tables...");
    
    for (int freq_idx = 0; freq_idx < SSVEP_FREQ_NUM; freq_idx++) {
        // 为每个频率分配灰度表
        _freq_tables[freq_idx].grayscale_table = (uint8_t *)malloc(GRAYSCALE_TABLE_SIZE);
        _freq_tables[freq_idx].table_size = GRAYSCALE_TABLE_SIZE;
        _freq_tables[freq_idx].period_ms = FREQ_PERIOD_MS[freq_idx];
        
        if (_freq_tables[freq_idx].grayscale_table == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate grayscale table for frequency %d Hz", FREQ_HZ[freq_idx]);
            continue;
        }
        
        // 使用正弦波生成灰度值：128 + 127*sin(2*pi*i/table_size)
        // 这样会产生0-255的灰度值
        for (int i = 0; i < GRAYSCALE_TABLE_SIZE; i++) {
            float phase = (2.0f * 3.14159265f * i) / GRAYSCALE_TABLE_SIZE;
            float sine_value = sinf(phase);  // -1 to 1
            uint8_t gray_value = (uint8_t)(128.0f + 127.0f * sine_value);
            _freq_tables[freq_idx].grayscale_table[i] = gray_value;
        }
        
        ESP_LOGI(TAG, "Grayscale table for %d Hz initialized (period: %d ms)", 
                 FREQ_HZ[freq_idx], _freq_tables[freq_idx].period_ms);
    }
}

void SSVEPApp::createRectangles(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating rectangles at four corners...");
    
    // 定义四个角的坐标
    struct {
        lv_align_t align;
        int16_t x_ofs;
        int16_t y_ofs;
        ssvep_freq_t freq;
    } corner_config[4] = {
        {LV_ALIGN_TOP_LEFT, 10, 10, SSVEP_FREQ_8HZ},      // 左上 - 8Hz
        {LV_ALIGN_TOP_RIGHT, -10, 10, SSVEP_FREQ_10HZ},   // 右上 - 10Hz
        {LV_ALIGN_BOTTOM_LEFT, 10, -10, SSVEP_FREQ_12HZ}, // 左下 - 12Hz
        {LV_ALIGN_BOTTOM_RIGHT, -10, -10, SSVEP_FREQ_16HZ} // 右下 - 16Hz
    };
    
    uint32_t current_ms = esp_timer_get_time() / 1000;
    
    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        // 创建矩形
        _rects[i].rect = lv_obj_create(parent);
        lv_obj_set_size(_rects[i].rect, _rect_size, _rect_size);
        lv_obj_align(_rects[i].rect, corner_config[i].align, 
                    corner_config[i].x_ofs, corner_config[i].y_ofs);
        
        // 设置初始样式
        lv_obj_set_style_radius(_rects[i].rect, 8, 0);
        lv_obj_set_style_border_width(_rects[i].rect, 2, 0);
        lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0x666666), 0);
        lv_obj_clear_flag(_rects[i].rect, LV_OBJ_FLAG_SCROLLABLE);
        
        // 初始化状态
        _rects[i].freq = corner_config[i].freq;
        _rects[i].current_index = 0;
        _rects[i].phase_start_ms = current_ms;
        _rects[i].active = true;
        
        // 设置初始颜色
        uint8_t gray_val = getGrayscaleValue(_rects[i].freq, current_ms);
        lv_color_t color = grayscaleToColor(gray_val);
        lv_obj_set_style_bg_color(_rects[i].rect, color, 0);
        
        ESP_LOGI(TAG, "Rectangle %d created at corner %d with frequency %d Hz", 
                 i, i, FREQ_HZ[corner_config[i].freq]);
    }
}

void SSVEPApp::createFeedbackArea(lv_obj_t *parent)
{
    ESP_LOGI(TAG, "Creating feedback area...");
    
    // 在中心创建反馈标签
    lv_obj_t *feedback_label = lv_label_create(parent);
    lv_label_set_text(feedback_label, "BCI Feedback");
    lv_obj_set_style_text_color(feedback_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(feedback_label, &lv_font_montserrat_20, 0);
    lv_obj_align(feedback_label, LV_ALIGN_CENTER, 0, -20);
    
    // 创建频率信息标签
    lv_obj_t *freq_label = lv_label_create(parent);
    lv_label_set_text(freq_label, "8Hz  10Hz  12Hz  16Hz");
    lv_obj_set_style_text_color(freq_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(freq_label, &lv_font_montserrat_16, 0);
    lv_obj_align(freq_label, LV_ALIGN_CENTER, 0, 20);
    
    ESP_LOGI(TAG, "Feedback area created");
}

uint8_t SSVEPApp::getGrayscaleValue(ssvep_freq_t freq, uint32_t current_ms)
{
    if (freq >= SSVEP_FREQ_NUM || _freq_tables[freq].grayscale_table == nullptr) {
        return 128;  // 返回中间灰度值
    }
    
    // 使用微秒级时间精度以提高频率精度
    uint64_t current_us = esp_timer_get_time();
    uint64_t elapsed_us = current_us - (_rects[freq].phase_start_ms * 1000ULL);
    uint64_t period_us = (uint64_t)_freq_tables[freq].period_ms * 1000ULL;
    
    // 计算在周期中的进度（0-GRAYSCALE_TABLE_SIZE-1）
    uint32_t phase_position = (uint32_t)((elapsed_us * GRAYSCALE_TABLE_SIZE) / period_us);
    phase_position %= GRAYSCALE_TABLE_SIZE;  // 取模保证循环
    
    // 获取当前和下一个灰度值用于线性插值
    uint32_t next_position = (phase_position + 1) % GRAYSCALE_TABLE_SIZE;
    uint8_t current_val = _freq_tables[freq].grayscale_table[phase_position];
    uint8_t next_val = _freq_tables[freq].grayscale_table[next_position];
    
    // 计算插值系数（用于更平滑的过渡）
    uint32_t frac = (uint32_t)(((elapsed_us * GRAYSCALE_TABLE_SIZE) % period_us) * 256 / period_us);
    
    // 线性插值
    uint8_t interpolated = current_val + ((int16_t)next_val - current_val) * frac / 256;
    
    return interpolated;
}

void SSVEPApp::updateAllRectangles(uint32_t current_ms)
{
    // 检查反馈是否超时
    if (_feedback_freq < SSVEP_FREQ_NUM) {
        if (current_ms - _feedback_start_time > FEEDBACK_DURATION_MS) {
            _feedback_freq = (ssvep_freq_t)(-1);
        }
    }
    
    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        if (!_rects[i].active || _rects[i].rect == nullptr) {
            continue;
        }
        
        uint8_t gray_val = getGrayscaleValue(_rects[i].freq, current_ms);
        lv_color_t color = grayscaleToColor(gray_val);
        lv_obj_set_style_bg_color(_rects[i].rect, color, 0);
        
        // 处理反馈：如果检测到该频率，显示绿色边框
        if (_feedback_freq == _rects[i].freq) {
            // 绿色边框（反馈）
            lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0x00ff00), 0);
            lv_obj_set_style_border_width(_rects[i].rect, 4, 0);
        } else {
            // 恢复默认灰色边框
            lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0x666666), 0);
            lv_obj_set_style_border_width(_rects[i].rect, 2, 0);
        }
    }
}

lv_color_t SSVEPApp::grayscaleToColor(uint8_t value)
{
    // 将灰度值转换为RGB颜色（全白=255时为白色，0时为黑色）
    return lv_color_make(value, value, value);
}

void SSVEPApp::updateTaskEntry(void *arg)
{
    SSVEPApp *app = static_cast<SSVEPApp *>(arg);
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t update_period = pdMS_TO_TICKS(UPDATE_PERIOD_MS);
    
    ESP_LOGI(TAG, "Update task started");
    
    while (app->_task_running) {
        uint32_t current_ms = esp_timer_get_time() / 1000;
        
        // 仅在未暂停时更新
        if (!app->_is_paused) {
            app->updateAllRectangles(current_ms);
        }
        
        // 精确延时
        vTaskDelayUntil(&last_wake_time, update_period);
    }
    
    ESP_LOGI(TAG, "Update task stopped");
    vTaskDelete(nullptr);
}

void SSVEPApp::setFeedback(ssvep_freq_t detected_freq)
{
    if (detected_freq >= SSVEP_FREQ_NUM) {
        return;
    }
    
    _feedback_freq = detected_freq;
    _feedback_start_time = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "Feedback detected: %d Hz", FREQ_HZ[detected_freq]);
}

void SSVEPApp::clearFeedback(void)
{
    _feedback_freq = (ssvep_freq_t)(-1);
    _feedback_start_time = 0;
}
