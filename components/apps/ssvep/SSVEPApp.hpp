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
#include "SSVEPSpiSlaveLink.hpp"
#include <atomic>

typedef enum {
    SSVEP_FREQ_8HZ = 0,
    SSVEP_FREQ_10HZ = 1,
    SSVEP_FREQ_12HZ = 2,
    SSVEP_FREQ_14HZ = 3,
    SSVEP_FREQ_NUM = 4
} ssvep_freq_t;

typedef struct {
    uint8_t *grayscale_table;
    uint16_t table_size;
    uint16_t period_ms;
} ssvep_freq_table_t;

typedef struct {
    lv_obj_t *rect;
    ssvep_freq_t freq;
    uint16_t current_index;
    uint32_t phase_start_ms;
    bool active;
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
    ssvep_freq_table_t _freq_tables[SSVEP_FREQ_NUM];
    ssvep_rect_t _rects[SSVEP_FREQ_NUM];
    lv_obj_t *_app_area;

    lv_obj_t *_feedback_rects[SSVEP_FREQ_NUM];
    lv_obj_t *_test_buttons[SSVEP_FREQ_NUM];

    TaskHandle_t _update_task;
    bool _task_running;
    bool _is_paused;
    uint32_t _feedback_packet_count;
    SSVEPSpiSlaveLink _spi_link;

    static constexpr uint32_t UPDATE_PERIOD_MS = 10;

    uint16_t _rect_size;

    lv_obj_t *_freq_label;
    lv_obj_t *_test_button_labels[SSVEP_FREQ_NUM];

    void initGrayscaleTables(void);
    void createRectangles(lv_obj_t *parent);
    void createFeedbackArea(lv_obj_t *parent);
    void createTestButtons(lv_obj_t *parent);
    uint8_t getGrayscaleValue(ssvep_freq_t freq, uint32_t current_ms);
    void updateAllRectangles(uint32_t current_ms);
    void setFeedback(ssvep_freq_t detected_freq);
    void clearFeedback(void);
    void resetUiState(void);
    void destroyUiLocked(void);
    bool handleIncomingPacket(const SpiResultPacket &packet, bool from_test_button);

    static void updateTaskEntry(void *arg);
    static bool packetCallback(void *ctx, const SpiResultPacket &packet, bool from_test_button);

    lv_color_t grayscaleToColor(uint8_t value);

    static void testButtonEventHandler(lv_event_t *e);

private:
      // 【关键修复 3】：使用 std::atomic 包装跨线程共享变量
    std::atomic<ssvep_freq_t> _feedback_freq;
    std::atomic<uint32_t> _feedback_start_time;
    std::atomic<uint32_t> _last_event_time_ms;
    static constexpr uint32_t FEEDBACK_DURATION_MS = 1000;
    static constexpr uint32_t EVENT_DEBOUNCE_MS = 1000;
};
