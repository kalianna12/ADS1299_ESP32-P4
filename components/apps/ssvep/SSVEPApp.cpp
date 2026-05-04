/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SSVEPApp.hpp"
#include <cmath>
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lv_adapter.h"
#include "assets/img_app_ssvep.c"

static const char *TAG = "SSVEPApp";
static constexpr bool kEnableSpiSlaveLink = true;

static constexpr uint16_t FREQ_HZ[SSVEP_FREQ_NUM] = {
    8, 10, 12, 14
};

static constexpr uint16_t FREQ_PERIOD_MS[SSVEP_FREQ_NUM] = {
    125, 100, 83, 71
};

static constexpr uint16_t GRAYSCALE_TABLE_SIZE = 64;

SSVEPApp::SSVEPApp():
    ESP_Brookesia_PhoneApp("SSVEP", &img_app_ssvep, true),
    _app_area(nullptr),
    _update_task(nullptr),
    _task_running(false),
    _is_paused(false),
    _feedback_packet_count(0),
    _rect_size(300),
    _freq_label(nullptr),
    _feedback_freq(static_cast<ssvep_freq_t>(-1)),
    _feedback_start_time(0),
    _last_event_time_ms(0)
{
    std::memset(_freq_tables, 0, sizeof(_freq_tables));
    std::memset(_rects, 0, sizeof(_rects));
    std::memset(_feedback_rects, 0, sizeof(_feedback_rects));
    std::memset(_test_buttons, 0, sizeof(_test_buttons));
    std::memset(_test_button_labels, 0, sizeof(_test_button_labels));
}

SSVEPApp::~SSVEPApp()
{
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
    initGrayscaleTables();
    return true;
}

bool SSVEPApp::run(void)
{
    ESP_LOGI(TAG, "SSVEP App running...");
    resetUiState();
    clearFeedback();
    _is_paused = false;
    _feedback_packet_count = 0;

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    _app_area = lv_obj_create(screen);
    // Set app area to fill screen but leave space for status bar at top
    lv_obj_set_size(_app_area, lv_obj_get_width(screen), lv_obj_get_height(screen) - 60);
    lv_obj_align(_app_area, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_radius(_app_area, 0, 0);
    lv_obj_set_style_border_width(_app_area, 0, 0);
    lv_obj_set_style_bg_color(_app_area, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(_app_area, 0, 0);
    lv_obj_clear_flag(_app_area, LV_OBJ_FLAG_SCROLLABLE);

    _rect_size = 170;

    createRectangles(_app_area);
    createTestButtons(_app_area);

    _task_running = true;
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        updateTaskEntry,
        "ssvep_update",
        4096,
        this,
        3,
        &_update_task,
        1
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create update task");
        _task_running = false;
        _update_task = nullptr;
        return false;
    }

    if (kEnableSpiSlaveLink) {
        if (!_spi_link.start(this, &SSVEPApp::packetCallback)) {
            ESP_LOGE(TAG, "Failed to start SPI slave link");
            _is_paused = true;
            _task_running = false;
            for (int i = 0; (i < 100) && (_update_task != nullptr); i++) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            if (esp_lv_adapter_lock(-1) == ESP_OK) {
                destroyUiLocked();
                esp_lv_adapter_unlock();
            }
            return false;
        }
    } else {
        ESP_LOGW(TAG, "SPI slave link disabled for isolation test");
    }

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

    _is_paused = true;
    _task_running = false;
    if (kEnableSpiSlaveLink) {
        _spi_link.stop();
    }

    for (int i = 0; (i < 100) && (_update_task != nullptr); i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        destroyUiLocked();
        esp_lv_adapter_unlock();
    }

    clearFeedback();
    resetUiState();
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
        if (_freq_tables[freq_idx].grayscale_table == nullptr) {
            _freq_tables[freq_idx].grayscale_table = static_cast<uint8_t *>(malloc(GRAYSCALE_TABLE_SIZE));
        }
        _freq_tables[freq_idx].table_size = GRAYSCALE_TABLE_SIZE;
        _freq_tables[freq_idx].period_ms = FREQ_PERIOD_MS[freq_idx];

        if (_freq_tables[freq_idx].grayscale_table == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate grayscale table for frequency %d Hz", FREQ_HZ[freq_idx]);
            continue;
        }

        for (int i = 0; i < GRAYSCALE_TABLE_SIZE; i++) {
            float phase = (2.0f * 3.14159265f * i) / GRAYSCALE_TABLE_SIZE;
            float sine_value = sinf(phase);
            uint8_t gray_value = static_cast<uint8_t>((sine_value + 1.0f) * 127.5f);
            _freq_tables[freq_idx].grayscale_table[i] = gray_value;
        }

        ESP_LOGI(TAG, "Grayscale table for %d Hz initialized (period: %d ms)",
                 FREQ_HZ[freq_idx], _freq_tables[freq_idx].period_ms);
    }
}

void SSVEPApp::createRectangles(lv_obj_t *parent)
{
    struct {
        lv_align_t align;
        int16_t x_ofs;
        int16_t y_ofs;
        ssvep_freq_t freq;
    } corner_config[SSVEP_FREQ_NUM] = {
         {LV_ALIGN_TOP_LEFT, 10, 10, SSVEP_FREQ_8HZ},       // 左上
         {LV_ALIGN_TOP_RIGHT, -10, 10, SSVEP_FREQ_10HZ},    // 右上
         {LV_ALIGN_BOTTOM_LEFT, 10, -10, SSVEP_FREQ_12HZ},  // 左下
         {LV_ALIGN_BOTTOM_RIGHT, -10, -10, SSVEP_FREQ_14HZ} // 右下
    };

    uint32_t current_ms = esp_timer_get_time() / 1000;

    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        _rects[i].rect = lv_obj_create(parent);
        lv_obj_set_size(_rects[i].rect, _rect_size, _rect_size);
        lv_obj_align(_rects[i].rect, corner_config[i].align, corner_config[i].x_ofs, corner_config[i].y_ofs);
        lv_obj_set_style_radius(_rects[i].rect, 8, 0);
        lv_obj_set_style_border_width(_rects[i].rect, 2, 0);
        lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0x666666), 0);
        lv_obj_clear_flag(_rects[i].rect, LV_OBJ_FLAG_SCROLLABLE);

        _rects[i].freq = corner_config[i].freq;
        _rects[i].current_index = 0;
        _rects[i].phase_start_ms = current_ms;
        _rects[i].active = true;

        uint8_t gray_val = getGrayscaleValue(_rects[i].freq, current_ms);
        lv_obj_set_style_bg_color(_rects[i].rect, grayscaleToColor(gray_val), 0);
    }
}

void SSVEPApp::createFeedbackArea(lv_obj_t *parent)
{
    lv_obj_t *feedback_label = lv_label_create(parent);
    lv_label_set_text(feedback_label, "BCI Feedback");
    lv_obj_set_style_text_color(feedback_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(feedback_label, &lv_font_montserrat_20, 0);
    lv_obj_align(feedback_label, LV_ALIGN_CENTER, 0, -20);

    _freq_label = lv_label_create(parent);
    lv_label_set_text_fmt(_freq_label, "%dHz  %dHz  %dHz  %dHz",
                          FREQ_HZ[0],
                          FREQ_HZ[1],
                          FREQ_HZ[2],
                          FREQ_HZ[3]);
    lv_obj_set_style_text_color(_freq_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(_freq_label, &lv_font_montserrat_16, 0);
    lv_obj_align(_freq_label, LV_ALIGN_CENTER, 0, 20);
}

void SSVEPApp::createTestButtons(lv_obj_t *parent)
{
    lv_color_t button_colors[SSVEP_FREQ_NUM] = {
        lv_color_hex(0x4CAF50),
        lv_color_hex(0x2196F3),
        lv_color_hex(0xFF9800),
        lv_color_hex(0xF44336)
    };

    struct {
        lv_align_t align;
        int16_t x_ofs;
        int16_t y_ofs;
    } button_positions[SSVEP_FREQ_NUM] = {
        {LV_ALIGN_TOP_LEFT, 60, 200},        // 左上方块下面
        {LV_ALIGN_TOP_RIGHT, -60, 200},      // 右上方块下面
        {LV_ALIGN_BOTTOM_LEFT, 60, -200},    // 左下方块上面
        {LV_ALIGN_BOTTOM_RIGHT, -60, -200}   // 右下方块上面
    };

    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        _test_buttons[i] = lv_btn_create(parent);
        lv_obj_set_size(_test_buttons[i], 60, 30);

        lv_obj_align(_test_buttons[i], button_positions[i].align, button_positions[i].x_ofs, button_positions[i].y_ofs);
        lv_obj_set_style_bg_color(_test_buttons[i], button_colors[i], 0);
        lv_obj_set_style_border_width(_test_buttons[i], 2, 0);
        lv_obj_set_style_border_color(_test_buttons[i], lv_color_hex(0x333333), 0);
        lv_obj_set_style_radius(_test_buttons[i], 8, 0);

        lv_obj_t *label = lv_label_create(_test_buttons[i]);
        lv_label_set_text_fmt(label, "%dHz", FREQ_HZ[i]);
        lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(_test_buttons[i], &lv_font_montserrat_16, 0);
        lv_obj_center(label);
        _test_button_labels[i] = label;

        lv_obj_add_event_cb(_test_buttons[i], testButtonEventHandler, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(_test_buttons[i], reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
    }
}

void SSVEPApp::testButtonEventHandler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    SSVEPApp *app = static_cast<SSVEPApp *>(lv_event_get_user_data(e));
    if (app == nullptr) {
        return;
    }

    uintptr_t freq_idx = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(btn));
    if (freq_idx >= SSVEP_FREQ_NUM) {
        return;
    }

    SpiResultPacket packet = {};
    packet.flags = 0x01;
    packet.detected_index = static_cast<uint8_t>(freq_idx);
    packet.detected_hz = FREQ_HZ[freq_idx];
    packet.confidence = 1.0f;
    packet.correlations[freq_idx] = 1.0f;

    ESP_LOGI(TAG, "Local test button clicked: %u Hz", FREQ_HZ[freq_idx]);
    app->handleIncomingPacket(packet, true);
}


uint8_t SSVEPApp::getGrayscaleValue(ssvep_freq_t freq, uint32_t current_ms)
{
    (void)current_ms;
    if ((freq >= SSVEP_FREQ_NUM) || (_freq_tables[freq].grayscale_table == nullptr)) {
        return 128;
    }

    uint64_t current_us = esp_timer_get_time();
    uint64_t elapsed_us = current_us - (_rects[freq].phase_start_ms * 1000ULL);
    uint64_t period_us = static_cast<uint64_t>(_freq_tables[freq].period_ms) * 1000ULL;
    if (period_us == 0) {
        return 128;
    }

    uint32_t phase_position = static_cast<uint32_t>((elapsed_us * GRAYSCALE_TABLE_SIZE) / period_us);
    phase_position %= GRAYSCALE_TABLE_SIZE;

    uint32_t next_position = (phase_position + 1) % GRAYSCALE_TABLE_SIZE;
    uint8_t current_val = _freq_tables[freq].grayscale_table[phase_position];
    uint8_t next_val = _freq_tables[freq].grayscale_table[next_position];
    uint32_t frac = static_cast<uint32_t>(((elapsed_us * GRAYSCALE_TABLE_SIZE) % period_us) * 256 / period_us);

    return current_val + ((static_cast<int16_t>(next_val) - current_val) * frac / 256);
}

void SSVEPApp::updateAllRectangles(uint32_t current_ms)
{
    // 【优化3】：将 atomic 变量一次性读取到局部变量，保证此循环内的状态一致性
    ssvep_freq_t current_fb_freq = _feedback_freq.load();
    uint32_t start_time = _feedback_start_time.load();

    if (current_fb_freq < SSVEP_FREQ_NUM) {
        // 【优化4】：必须判断 current_ms >= start_time，防止多线程时间差导致的无符号整数下溢 BUG
        if (current_ms >= start_time && (current_ms - start_time > FEEDBACK_DURATION_MS)) {
            _feedback_freq.store(static_cast<ssvep_freq_t>(-1));
            current_fb_freq = static_cast<ssvep_freq_t>(-1); // 更新局部变量
        }
    }

    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        if (!_rects[i].active || (_rects[i].rect == nullptr)) {
            continue;
        }

        uint8_t gray_val = getGrayscaleValue(_rects[i].freq, current_ms);
        lv_obj_set_style_bg_color(_rects[i].rect, grayscaleToColor(gray_val), 0);

        // 使用一致的局部变量进行比较
        if (current_fb_freq == _rects[i].freq) {
            lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0x00ff00), 0);
            lv_obj_set_style_border_width(_rects[i].rect, 6, 0); // 建议改为6，视觉反馈更明显
        } else {
            lv_obj_set_style_border_color(_rects[i].rect, lv_color_hex(0x666666), 0);
            lv_obj_set_style_border_width(_rects[i].rect, 2, 0);
        }
    }
}

lv_color_t SSVEPApp::grayscaleToColor(uint8_t value)
{
    return lv_color_make(value, value, value);
}

void SSVEPApp::updateTaskEntry(void *arg)
{
    SSVEPApp *app = static_cast<SSVEPApp *>(arg);
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t update_period = pdMS_TO_TICKS(UPDATE_PERIOD_MS);

    ESP_LOGI(TAG, "Update task started");

    while (app->_task_running) {
        if (!app->_is_paused) {
            uint32_t current_ms = esp_timer_get_time() / 1000;
            if (esp_lv_adapter_lock(-1) == ESP_OK) {
                app->updateAllRectangles(current_ms);
                esp_lv_adapter_unlock();
            }
        }
        vTaskDelayUntil(&last_wake_time, update_period);
    }

    ESP_LOGI(TAG, "Update task stopped");
    app->_update_task = nullptr;
    vTaskDelete(nullptr);
}

bool SSVEPApp::handleIncomingPacket(const SpiResultPacket &packet, bool from_test_button)
{
    if (!_task_running || _app_area == nullptr) {
        ESP_LOGW(TAG, "Ignore packet because SSVEP UI is not active");
        return false;
    }
    if (packet.magic != SPI_RESULT_PACKET_MAGIC) {
        ESP_LOGW(TAG, "Ignore SPI packet with bad magic: 0x%04x", packet.magic);
        return false;
    }
    if (packet.version != SPI_RESULT_PACKET_VERSION) {
        ESP_LOGW(TAG, "Ignore SPI packet with bad version: %u", packet.version);
        return false;
    }
    if ((packet.flags & 0x01) == 0) {
        ESP_LOGW(TAG, "Ignore SPI packet without valid-result flag");
        return false;
    }
    if (packet.detected_index >= SSVEP_FREQ_NUM) {
        ESP_LOGW(TAG, "Ignore SPI packet with invalid index: %u", packet.detected_index);
        return false;
    }

    ++_feedback_packet_count;
    ESP_LOGI(TAG,
             "%s packet: idx=%u hz=%u conf=%.3f count=%lu",
             from_test_button ? "Test" : "SPI",
             packet.detected_index,
             packet.detected_hz,
             packet.confidence,
             static_cast<unsigned long>(_feedback_packet_count));

    uint32_t now_ms = esp_timer_get_time() / 1000;
    uint32_t last_event_ms = _last_event_time_ms.load();

    // 【优化1】：只要收到有效包，就刷新UI高亮时间，保证只要盯着看，绿框就一直亮着
    setFeedback(static_cast<ssvep_freq_t>(packet.detected_index));

    // 【优化2】：实际的动作（页面跳转等）依然走防抖逻辑，防止疯狂触发
    if (from_test_button || (now_ms - last_event_ms >= EVENT_DEBOUNCE_MS)) {
        _last_event_time_ms.store(now_ms);

        ESP_LOGW(TAG,
                 ">>> EVENT TRIGGERED: %s %u Hz <<<",
                 from_test_button ? "test button" : "debounced SPI result",
                 packet.detected_hz);

        // TODO: 这里写你的页面跳转 / 按钮事件逻辑
        switch (packet.detected_index) {
        case SSVEP_FREQ_8HZ:
            ESP_LOGW(TAG, "Action for 8Hz");
            // back();
            break;
        case SSVEP_FREQ_10HZ:
            ESP_LOGW(TAG, "Action for 10Hz");
            break;
        case SSVEP_FREQ_12HZ:
            ESP_LOGW(TAG, "Action for 12Hz");
            break;
        case SSVEP_FREQ_14HZ:
            ESP_LOGW(TAG, "Action for 14Hz");
            break;
        default:
            break;
        }
    } else {
        // 取消这里的日志，防止刷屏
        // ESP_LOGI(TAG, "Burst packet ignored before feedback.");
    }
    
    return true;
}

bool SSVEPApp::packetCallback(void *ctx, const SpiResultPacket &packet, bool from_test_button)
{
    auto *app = static_cast<SSVEPApp *>(ctx);
    if (app == nullptr) {
        return false;
    }
    return app->handleIncomingPacket(packet, from_test_button);
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
    _feedback_freq = static_cast<ssvep_freq_t>(-1);
    _feedback_start_time = 0;
    _last_event_time_ms = 0;
}

void SSVEPApp::resetUiState(void)
{
    _app_area = nullptr;
    _freq_label = nullptr;

    for (int i = 0; i < SSVEP_FREQ_NUM; i++) {
        _rects[i].rect = nullptr;
        _rects[i].current_index = 0;
        _rects[i].phase_start_ms = 0;
        _rects[i].active = false;
        _feedback_rects[i] = nullptr;
        _test_buttons[i] = nullptr;
        _test_button_labels[i] = nullptr;
    }
}

void SSVEPApp::destroyUiLocked(void)
{
    if (_app_area != nullptr) {
        lv_obj_del(_app_area);
        _app_area = nullptr;
    }
}
