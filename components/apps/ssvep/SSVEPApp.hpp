/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"

// SSVEP频率定义（Hz）
typedef enum {
    SSVEP_FREQ_8HZ = 0,
    SSVEP_FREQ_10HZ = 1,
    SSVEP_FREQ_12HZ = 2,
    SSVEP_FREQ_14HZ = 3,
    SSVEP_FREQ_NUM = 4
} ssvep_freq_t;

// 灰度查表结构
typedef struct {
    uint8_t *grayscale_table;  // 预计算的灰度值查表
    uint16_t table_size;        // 查表大小
    uint16_t period_ms;         // 频率周期（毫秒）
} ssvep_freq_table_t;

// 方块状态结构
typedef struct {
    lv_obj_t *rect;             // 矩形对象
    ssvep_freq_t freq;          // 频率类型
    uint16_t current_index;     // 当前灰度表索引
    uint32_t phase_start_ms;    // 相位开始时间
    bool active;                // 是否活跃
} ssvep_rect_t;

class SSVEPApp: public ESP_Brookesia_PhoneApp {
public:
    SSVEPApp();
    ~SSVEPApp();

    bool run(void) override;
    bool back(void) override;
    bool close(void) override;
    bool init(void) override;
    bool pause(void) override;
    bool resume(void) override;

private:
    // 灰度查表相关
    ssvep_freq_table_t _freq_tables[SSVEP_FREQ_NUM];
    
    // 方块对象
    ssvep_rect_t _rects[SSVEP_FREQ_NUM];
    lv_obj_t *_app_area;        // 应用显示区域
    
    // 回显区域
    lv_obj_t *_feedback_rects[SSVEP_FREQ_NUM];  // 用于回显的方框
    lv_obj_t *_test_buttons[SSVEP_FREQ_NUM];    // 测试按钮
    
    // FreeRTOS任务
    TaskHandle_t _update_task;
    bool _task_running;
    bool _is_paused;
    
    // 任务更新周期（毫秒） - 使用10ms周期来精确控制
    static constexpr uint32_t UPDATE_PERIOD_MS = 10;
    
    // 应用显示区域尺寸
    uint16_t _app_width;
    uint16_t _app_height;
    uint16_t _rect_size;
    
    // 方法
    void initGrayscaleTables(void);
    void createRectangles(lv_obj_t *parent);
    void createFeedbackArea(lv_obj_t *parent);
    void createTestButtons(lv_obj_t *parent);
    uint8_t getGrayscaleValue(ssvep_freq_t freq, uint32_t current_ms);
    void updateAllRectangles(uint32_t current_ms);
    void setFeedback(ssvep_freq_t detected_freq);  // 用于设置检测到的频率反馈
    void clearFeedback(void);  // 清除反馈
    
    // FreeRTOS任务入口
    static void updateTaskEntry(void *arg);
    
    // 颜色计算
    lv_color_t grayscaleToColor(uint8_t value);
    
    // 事件处理
    static void testButtonEventHandler(lv_event_t *e);
    
private:
    ssvep_freq_t _feedback_freq;  // 检测到的频率反馈
    uint32_t _feedback_start_time;  // 反馈开始时间
    static constexpr uint32_t FEEDBACK_DURATION_MS = 500;  // 反馈持续时间
};
