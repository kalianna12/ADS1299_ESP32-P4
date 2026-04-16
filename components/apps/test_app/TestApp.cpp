/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "TestApp.hpp"

static constexpr lv_coord_t kCardWidth = 520;
static constexpr lv_coord_t kCardHeight = 452;
static constexpr lv_coord_t kPreviewMinSize = 112;
static constexpr lv_coord_t kPreviewMaxSize = 160;
static constexpr lv_coord_t kPreviewIdleMinSize = 120;
static constexpr lv_coord_t kPreviewIdleMaxSize = 136;
static constexpr const char *kPreviewMessages[] = {"HELLO", "TOUCH", "LVGL", "READY"};

TestApp::TestApp():
    ESP_Brookesia_PhoneApp("Test App", nullptr, true),
    _status_label(nullptr),
    _value_label(nullptr),
    _preview_box(nullptr),
    _preview_text(nullptr),
    _accent_bar(nullptr),
    _tap_count(0)
{
}

TestApp::~TestApp()
{
}

void TestApp::preview_size_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_size(static_cast<lv_obj_t *>(obj), value, value);
    lv_obj_set_style_radius(static_cast<lv_obj_t *>(obj), LV_RADIUS_CIRCLE, 0);
}

bool TestApp::run(void)
{
    lv_obj_t *screen = lv_scr_act();

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080808), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_size(card, kCardWidth, kCardHeight);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 28, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x404040), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_shadow_width(card, 28, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(card, 28, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "TEST APP");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf5f5f5), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *subtitle = lv_label_create(card);
    lv_label_set_text(subtitle, "TEST APP");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xa0a0a0), 0);
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *tileview = lv_tileview_create(card);
    lv_obj_set_size(tileview, 456, 286);
    lv_obj_align(tileview, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tileview, 0, 0);
    lv_obj_set_style_pad_all(tileview, 0, 0);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_t *page_preview = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
    lv_obj_t *page_controls = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
    lv_obj_t *page_action = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR);

    for (lv_obj_t *page : {page_preview, page_controls, page_action}) {
        lv_obj_set_style_bg_color(page, lv_color_hex(0x090909), 0);
        lv_obj_set_style_border_width(page, 1, 0);
        lv_obj_set_style_border_color(page, lv_color_hex(0x303030), 0);
        lv_obj_set_style_radius(page, 26, 0);
        lv_obj_set_style_pad_all(page, 18, 0);
        lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_t *preview_title = lv_label_create(page_preview);
    lv_label_set_text(preview_title, "Preview");
    lv_obj_set_style_text_font(preview_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(preview_title, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(preview_title, LV_ALIGN_TOP_LEFT, 0, 0);

    _status_label = lv_label_create(page_preview);
    lv_label_set_text(_status_label, "Mode: Dark");
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0xd0d0d0), 0);
    lv_obj_align(_status_label, LV_ALIGN_TOP_LEFT, 0, 48);

    _value_label = lv_label_create(page_preview);
    lv_label_set_text(_value_label, "Intensity 60%");
    lv_obj_set_style_text_font(_value_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(_value_label, lv_color_hex(0xd0d0d0), 0);
    lv_obj_align(_value_label, LV_ALIGN_TOP_RIGHT, 0, 48);

    lv_obj_t *preview_wrap = lv_obj_create(page_preview);
    lv_obj_set_size(preview_wrap, 404, 158);
    lv_obj_align(preview_wrap, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_radius(preview_wrap, 28, 0);
    lv_obj_set_style_border_width(preview_wrap, 1, 0);
    lv_obj_set_style_border_color(preview_wrap, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_color(preview_wrap, lv_color_hex(0x090909), 0);
    lv_obj_set_style_pad_all(preview_wrap, 0, 0);
    lv_obj_clear_flag(preview_wrap, LV_OBJ_FLAG_SCROLLABLE);

    _accent_bar = lv_obj_create(preview_wrap);
    lv_obj_set_size(_accent_bar, 232, 10);
    lv_obj_align(_accent_bar, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_radius(_accent_bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(_accent_bar, 0, 0);
    lv_obj_set_style_bg_color(_accent_bar, lv_color_hex(0xffffff), 0);

    _preview_box = lv_obj_create(preview_wrap);
    lv_obj_set_size(_preview_box, 128, 128);
    lv_obj_align(_preview_box, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_radius(_preview_box, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(_preview_box, 0, 0);
    lv_obj_set_style_bg_color(_preview_box, lv_color_hex(0xf2f2f2), 0);
    lv_obj_clear_flag(_preview_box, LV_OBJ_FLAG_SCROLLABLE);

    _preview_text = lv_label_create(_preview_box);
    lv_label_set_text(_preview_text, "HELLO");
    lv_obj_set_style_text_font(_preview_text, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_preview_text, lv_color_hex(0x121212), 0);
    lv_obj_center(_preview_text);

    lv_obj_t *controls_title = lv_label_create(page_controls);
    lv_label_set_text(controls_title, "Controls");
    lv_obj_set_style_text_font(controls_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(controls_title, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(controls_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *toggle_label = lv_label_create(page_controls);
    lv_label_set_text(toggle_label, "Light mode");
    lv_obj_set_style_text_font(toggle_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(toggle_label, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(toggle_label, LV_ALIGN_TOP_LEFT, 0, 56);

    lv_obj_t *mode_switch = lv_switch_create(page_controls);
    lv_obj_align(mode_switch, LV_ALIGN_TOP_RIGHT, 0, 50);
    lv_obj_add_event_cb(mode_switch, onSwitchChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *slider_title = lv_label_create(page_controls);
    lv_label_set_text(slider_title, "Intensity");
    lv_obj_set_style_text_font(slider_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(slider_title, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(slider_title, LV_ALIGN_TOP_LEFT, 0, 126);

    lv_obj_t *slider = lv_slider_create(page_controls);
    lv_obj_set_width(slider, 404);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 168);
    lv_slider_set_range(slider, 20, 100);
    lv_slider_set_value(slider, 60, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, onSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *action_title = lv_label_create(page_action);
    lv_label_set_text(action_title, "Action");
    lv_obj_set_style_text_font(action_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(action_title, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(action_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *action_desc = lv_label_create(page_action);
    lv_label_set_text(action_desc, "Tap the button to cycle the center text and trigger a resize animation.");
    lv_obj_set_width(action_desc, 404);
    lv_obj_set_style_text_font(action_desc, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(action_desc, lv_color_hex(0xb0b0b0), 0);
    lv_obj_align(action_desc, LV_ALIGN_TOP_LEFT, 0, 48);

    lv_obj_t *tap_btn = lv_btn_create(page_action);
    lv_obj_set_size(tap_btn, 210, 58);
    lv_obj_align(tap_btn, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_style_radius(tap_btn, 24, 0);
    lv_obj_set_style_bg_color(tap_btn, lv_color_hex(0xf2f2f2), 0);
    lv_obj_set_style_text_color(tap_btn, lv_color_hex(0x111111), 0);
    lv_obj_add_event_cb(tap_btn, onButtonClicked, LV_EVENT_CLICKED, this);

    lv_obj_t *tap_label = lv_label_create(tap_btn);
    lv_label_set_text(tap_label, "Tap Me");
    lv_obj_set_style_text_font(tap_label, &lv_font_montserrat_20, 0);
    lv_obj_center(tap_label);

    updateIntensity(60);
    setInteractiveMode(false);

    lv_anim_t idle_anim;
    lv_anim_init(&idle_anim);
    lv_anim_set_var(&idle_anim, _preview_box);
    lv_anim_set_values(&idle_anim, kPreviewIdleMinSize, kPreviewIdleMaxSize);
    lv_anim_set_time(&idle_anim, 1100);
    lv_anim_set_playback_time(&idle_anim, 1100);
    lv_anim_set_repeat_count(&idle_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&idle_anim, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&idle_anim, preview_size_anim_cb);
    lv_anim_start(&idle_anim);

    return true;
}

void TestApp::onSliderChanged(lv_event_t *e)
{
    auto *self = static_cast<TestApp *>(lv_event_get_user_data(e));
    self->updateIntensity(lv_slider_get_value(static_cast<lv_obj_t *>(lv_event_get_target(e))));
}

void TestApp::onSwitchChanged(lv_event_t *e)
{
    auto *self = static_cast<TestApp *>(lv_event_get_user_data(e));
    lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_target(e));

    self->setInteractiveMode(lv_obj_has_state(sw, LV_STATE_CHECKED));
}

void TestApp::onButtonClicked(lv_event_t *e)
{
    auto *self = static_cast<TestApp *>(lv_event_get_user_data(e));
    self->_tap_count++;
    lv_label_set_text(self->_status_label, self->_tap_count % 2 ? "Mode: Touched" : "Mode: Interactive");
    lv_label_set_text(self->_preview_text, kPreviewMessages[self->_tap_count % 4]);
    self->animatePreview();
}

void TestApp::updateIntensity(int32_t value)
{
    if ((_accent_bar == nullptr) || (_value_label == nullptr)) {
        return;
    }

    const lv_coord_t width = 120 + static_cast<lv_coord_t>(value * 2.2f);
    const lv_opa_t opa = static_cast<lv_opa_t>(80 + value);
    char text[24] = {};

    lv_snprintf(text, sizeof(text), "Intensity %d%%", value);
    lv_label_set_text(_value_label, text);
    lv_obj_set_width(_accent_bar, width);
    lv_obj_set_style_opa(_accent_bar, opa, 0);
}

void TestApp::setInteractiveMode(bool enabled)
{
    if ((_preview_box == nullptr) || (_preview_text == nullptr) || (_status_label == nullptr)) {
        return;
    }

    if (enabled) {
        lv_label_set_text(_status_label, "Mode: Light");
        lv_obj_set_style_bg_color(_preview_box, lv_color_hex(0x101010), 0);
        lv_obj_set_style_border_width(_preview_box, 2, 0);
        lv_obj_set_style_border_color(_preview_box, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_color(_preview_text, lv_color_hex(0xf5f5f5), 0);
    } else {
        lv_label_set_text(_status_label, "Mode: Dark");
        lv_obj_set_style_bg_color(_preview_box, lv_color_hex(0xf2f2f2), 0);
        lv_obj_set_style_border_width(_preview_box, 0, 0);
        lv_obj_set_style_text_color(_preview_text, lv_color_hex(0x121212), 0);
    }
}

void TestApp::animatePreview(void)
{
    if (_preview_box == nullptr) {
        return;
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, _preview_box);
    lv_anim_set_values(&anim, kPreviewMinSize, kPreviewMaxSize);
    lv_anim_set_time(&anim, 180);
    lv_anim_set_playback_time(&anim, 220);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, preview_size_anim_cb);
    lv_anim_start(&anim);
}

bool TestApp::back(void)
{
    notifyCoreClosed();

    return true;
}

bool TestApp::close(void)
{
    _status_label = nullptr;
    _value_label = nullptr;
    _preview_box = nullptr;
    _preview_text = nullptr;
    _accent_bar = nullptr;

    return true;
}

bool TestApp::init(void)
{
    return true;
}
