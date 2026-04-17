/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"

class TestApp: public ESP_Brookesia_PhoneApp {
public:
    TestApp();
    ~TestApp();

    bool run(void);
    bool back(void);
    bool close(void);
    bool init(void) override;

private:
    lv_obj_t *_status_label;
    lv_obj_t *_button_label;
    lv_obj_t *_refresh_label;
    lv_obj_t *_response_label;
    lv_obj_t *_response_panel;
    lv_obj_t *_spinner;

    TaskHandle_t _request_task;
    uint32_t _ui_generation;
    bool _request_running;

    static void onButtonClicked(lv_event_t *e);
    static void requestTaskEntry(void *arg);

    void startRequest(void);
    void setStatus(const char *text);
    void setResponseText(const char *text);
    void setLoading(bool loading);
    bool isUiValid(uint32_t generation) const;
    std::string performRequest(void) const;
};
