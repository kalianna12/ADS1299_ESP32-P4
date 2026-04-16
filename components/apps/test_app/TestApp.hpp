/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

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
    lv_obj_t *_value_label;
    lv_obj_t *_preview_box;
    lv_obj_t *_preview_text;
    lv_obj_t *_accent_bar;
    uint32_t _tap_count;

    static void preview_size_anim_cb(void *obj, int32_t value);
    static void onSliderChanged(lv_event_t *e);
    static void onSwitchChanged(lv_event_t *e);
    static void onButtonClicked(lv_event_t *e);

    void updateIntensity(int32_t value);
    void setInteractiveMode(bool enabled);
    void animatePreview(void);
};
