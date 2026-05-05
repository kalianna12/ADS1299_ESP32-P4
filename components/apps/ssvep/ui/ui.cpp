/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui.h"
#include <cstdio>
#include "esp_log.h"

extern "C" {
    extern const lv_img_dsc_t call_icon;
    extern const lv_img_dsc_t rest_icon;
    extern const lv_img_dsc_t live_icon;
    extern const lv_img_dsc_t back_icon;
    extern const lv_img_dsc_t wrong_icon;
    extern const lv_img_dsc_t ok_icon;
    extern const lv_img_dsc_t status_icon;
    extern const lv_img_dsc_t last_icon;
    extern const lv_img_dsc_t conf_icon;
}

static const char *TAG = "SSVEPPanelUi";

// Icon resources
static const lv_img_dsc_t *const kTileImages[4] = {
    &call_icon,
    &rest_icon,
    &live_icon,
    &back_icon,
};

// ASCII labels: avoid CJK missing glyph boxes with lv_font_montserrat_xx
static const char *const kTileTitles[4] = {
    "CALL NURSE",
    "ROOM CTRL",
    "LIFE HELP",
    "BACK",
};

static const char *const kTileFreqs[4] = {
    "8 Hz",
    "10 Hz",
    "12 Hz",
    "14 Hz",
};

// Tile background colors
static const uint32_t kTileBgColors[4] = {
    0x1A0A0A,   // call - dark red
    0x070F1E,   // rest - dark blue
    0x071A0E,   // live - dark green
    0x2A1608,   // back - dark orange
};

// Tile border colors
static const uint32_t kTileBorderColors[4] = {
    0xC0392B,   // call - red
    0x2980B9,   // rest - blue
    0x27AE60,   // live - green
    0xF59E0B,   // back - orange
};

SSVEPPanelUi::SSVEPPanelUi():
    _parent(nullptr),
    _panel(nullptr),
    _tiles{nullptr, nullptr, nullptr, nullptr},
    _icon_boxes{nullptr, nullptr, nullptr, nullptr},
    _icons{nullptr, nullptr, nullptr, nullptr},
    _labels{nullptr, nullptr, nullptr, nullptr},
    _freq_labels{nullptr, nullptr, nullptr, nullptr},
    _status_bar(nullptr),
    _status_items{nullptr, nullptr, nullptr, nullptr},
    _status_icons{nullptr, nullptr, nullptr, nullptr},
    _status_labels{nullptr, nullptr, nullptr, nullptr},
    _status_separators{nullptr, nullptr, nullptr},
    _system_check_timer(nullptr),
    _current_page(Page::MAIN),
    _previous_page(Page::MAIN),
    _system_status(SystemStatus::WRONG),
    _system_checking(false),
    _ssvep_running(true),
    _action_cb(nullptr),
    _action_user_data(nullptr),
    _ssvep_running_cb(nullptr),
    _ssvep_running_user_data(nullptr)
{
    std::snprintf(_last_action, sizeof(_last_action), "%s", "--");
    std::snprintf(_conf_value, sizeof(_conf_value), "%s", "--");
}

bool SSVEPPanelUi::create(lv_obj_t *parent)
{
    if (parent == nullptr) {
        ESP_LOGE(TAG, "create(): parent is null");
        return false;
    }

    destroy();

    _parent = parent;

    _panel = lv_obj_create(parent);
    lv_obj_set_size(_panel, kPanelWidth, kPanelHeight);
    lv_obj_align(_panel, LV_ALIGN_CENTER, 0, -10);

    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x0B1730), 0);
    lv_obj_set_style_bg_opa(_panel, LV_OPA_90, 0);
    lv_obj_set_style_radius(_panel, 24, 0);
    lv_obj_set_style_clip_corner(_panel, true, 0);

    lv_obj_set_style_border_width(_panel, 2, 0);
    lv_obj_set_style_border_color(_panel, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_opa(_panel, LV_OPA_60, 0);

    lv_obj_set_style_shadow_width(_panel, 8, 0);
    lv_obj_set_style_shadow_color(_panel, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_shadow_opa(_panel, LV_OPA_20, 0);

    lv_obj_set_style_pad_all(_panel, 0, 0);
    lv_obj_clear_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_panel, LV_SCROLLBAR_MODE_OFF);

    showMainPage();
    startSystemWearCheck();

    ESP_LOGI(TAG, "Panel created (%dx%d)", kPanelWidth, kPanelHeight);
    return true;
}

void SSVEPPanelUi::destroy()
{
    if (_system_check_timer != nullptr) {
        lv_timer_del(_system_check_timer);
        _system_check_timer = nullptr;
    }

    if (_panel != nullptr) {
        lv_obj_del(_panel);
        _panel = nullptr;
    }

    _parent = nullptr;

    for (uint8_t i = 0; i < kTileCount; i++) {
        _tiles[i]       = nullptr;
        _icon_boxes[i]  = nullptr;
        _icons[i]       = nullptr;
        _labels[i]      = nullptr;
        _freq_labels[i] = nullptr;
    }

    _status_bar = nullptr;
    for (uint8_t i = 0; i < kStatusItemCount; i++) {
        _status_items[i] = nullptr;
        _status_icons[i] = nullptr;
        _status_labels[i] = nullptr;
    }
    for (uint8_t i = 0; i < kStatusItemCount - 1; i++) {
        _status_separators[i] = nullptr;
    }

    _current_page  = Page::MAIN;
    _previous_page = Page::MAIN;
    _system_status = SystemStatus::WRONG;
    _system_checking = false;
    _ssvep_running = true;
    std::snprintf(_last_action, sizeof(_last_action), "%s", "--");
    std::snprintf(_conf_value, sizeof(_conf_value), "%s", "--");
}

void SSVEPPanelUi::showMainPage()
{
    if (_panel == nullptr) {
        return;
    }

    _previous_page = _current_page;
    _current_page  = Page::MAIN;

    clearPanelContent();
    createMainTiles();
    createStatusBar();
}

void SSVEPPanelUi::showRestPage()
{
    if (_panel == nullptr) {
        return;
    }

    _previous_page = _current_page;
    _current_page  = Page::REST;

    clearPanelContent();
    createPageHeader("ROOM CONTROL", "Coming soon");
    createBackTileOnly();
    createStatusBar();
}

void SSVEPPanelUi::showLivePage()
{
    if (_panel == nullptr) {
        return;
    }

    _previous_page = _current_page;
    _current_page  = Page::LIVE;

    clearPanelContent();
    createPageHeader("LIFE SERVICE", "Coming soon");
    createBackTileOnly();
    createStatusBar();
}

void SSVEPPanelUi::goBack()
{
    showMainPage();
}

SSVEPPanelUi::Page SSVEPPanelUi::currentPage() const
{
    return _current_page;
}

void SSVEPPanelUi::setActionCallback(ActionCallback cb, void *user_data)
{
    _action_cb        = cb;
    _action_user_data = user_data;
}

void SSVEPPanelUi::setSsvepRunningCallback(SsvepRunningCallback cb, void *user_data)
{
    _ssvep_running_cb = cb;
    _ssvep_running_user_data = user_data;
}

void SSVEPPanelUi::updateLastAction(const char *action_name)
{
    if ((action_name == nullptr) || (action_name[0] == '\0')) {
        std::snprintf(_last_action, sizeof(_last_action), "%s", "--");
    } else if (action_name != _last_action) {
        std::snprintf(_last_action, sizeof(_last_action), "%s", action_name);
    }

    if (_status_labels[static_cast<uint8_t>(StatusItem::LAST)] != nullptr) {
        lv_label_set_text_fmt(_status_labels[static_cast<uint8_t>(StatusItem::LAST)], "LAST: %s", _last_action);
    }
}

void SSVEPPanelUi::updateConfValue(float conf)
{
    std::snprintf(_conf_value, sizeof(_conf_value), "%.2f", static_cast<double>(conf));
    if (_status_labels[static_cast<uint8_t>(StatusItem::CONF)] != nullptr) {
        lv_label_set_text_fmt(_status_labels[static_cast<uint8_t>(StatusItem::CONF)], "CONF: %s", _conf_value);
    }
}

void SSVEPPanelUi::updateConfValue(const char *conf)
{
    if ((conf == nullptr) || (conf[0] == '\0')) {
        std::snprintf(_conf_value, sizeof(_conf_value), "%s", "--");
    } else if (conf != _conf_value) {
        std::snprintf(_conf_value, sizeof(_conf_value), "%s", conf);
    }

    if (_status_labels[static_cast<uint8_t>(StatusItem::CONF)] != nullptr) {
        lv_label_set_text_fmt(_status_labels[static_cast<uint8_t>(StatusItem::CONF)], "CONF: %s", _conf_value);
    }
}

void SSVEPPanelUi::triggerByIndex(uint8_t index)
{
    if (index >= kTileCount) {
        ESP_LOGW(TAG, "triggerByIndex: invalid index %u", index);
        return;
    }

    handleTile(index);
}

void SSVEPPanelUi::clearPanelContent()
{
    if (_panel == nullptr) {
        return;
    }

    // Only clean the panel content. Never clean _app_area.
    lv_obj_clean(_panel);

    for (uint8_t i = 0; i < kTileCount; i++) {
        _tiles[i]       = nullptr;
        _icon_boxes[i]  = nullptr;
        _icons[i]       = nullptr;
        _labels[i]      = nullptr;
        _freq_labels[i] = nullptr;
    }
    _status_bar = nullptr;
    for (uint8_t i = 0; i < kStatusItemCount; i++) {
        _status_items[i] = nullptr;
        _status_icons[i] = nullptr;
        _status_labels[i] = nullptr;
    }
    for (uint8_t i = 0; i < kStatusItemCount - 1; i++) {
        _status_separators[i] = nullptr;
    }
}

void SSVEPPanelUi::createMainTiles()
{
    createTile(0, kLeftX,  kTopY);     // top-left: call
    createTile(1, kRightX, kTopY);     // top-right: room control
    createTile(2, kLeftX,  kBottomY);  // bottom-left: life help
    createTile(3, kRightX, kBottomY);  // bottom-right: back
}

void SSVEPPanelUi::createBackTileOnly()
{
    // Keep back tile fixed at panel bottom-right.
    createTile(3, kRightX, kBottomY);
}

void SSVEPPanelUi::createPageHeader(const char *title, const char *subtitle)
{
    if (_panel == nullptr) {
        return;
    }

    lv_obj_t *title_lbl = lv_label_create(_panel);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xE0EAF4), 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 62);
    makeChildPassThrough(title_lbl);

    lv_obj_t *sub_lbl = lv_label_create(_panel);
    lv_label_set_text(sub_lbl, subtitle);
    lv_obj_set_style_text_color(sub_lbl, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(sub_lbl, LV_ALIGN_TOP_MID, 0, 98);
    makeChildPassThrough(sub_lbl);
}

void SSVEPPanelUi::createStatusBar()
{
    if (_panel == nullptr) {
        return;
    }

    _status_bar = lv_obj_create(_panel);
    lv_obj_remove_style_all(_status_bar);
    lv_obj_set_size(_status_bar, kStatusBarW, kStatusBarH);
    lv_obj_align(_status_bar, LV_ALIGN_TOP_MID, 0, kStatusBarY);
    lv_obj_set_style_radius(_status_bar, 10, 0);
    lv_obj_set_style_clip_corner(_status_bar, true, 0);
    lv_obj_set_style_bg_color(_status_bar, lv_color_hex(0x061225), 0);
    lv_obj_set_style_bg_opa(_status_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(_status_bar, 1, 0);
    lv_obj_set_style_border_color(_status_bar, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_opa(_status_bar, LV_OPA_60, 0);
    lv_obj_set_style_shadow_width(_status_bar, 8, 0);
    lv_obj_set_style_shadow_color(_status_bar, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_shadow_opa(_status_bar, LV_OPA_20, 0);
    lv_obj_clear_flag(_status_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(_status_bar, LV_SCROLLBAR_MODE_OFF);

    createStatusItem(static_cast<uint8_t>(StatusItem::SYSTEM), &wrong_icon, "SYSTEM test...");
    createStatusItem(static_cast<uint8_t>(StatusItem::SSVEP), &status_icon, "SSVEP RUNNING");
    createStatusItem(static_cast<uint8_t>(StatusItem::LAST), &last_icon, "LAST: --");
    createStatusItem(static_cast<uint8_t>(StatusItem::CONF), &conf_icon, "CONF: --");

    const uint16_t item_w = kStatusBarW / kStatusItemCount;
    for (uint8_t i = 0; i < kStatusItemCount - 1; i++) {
        _status_separators[i] = lv_obj_create(_status_bar);
        lv_obj_remove_style_all(_status_separators[i]);
        lv_obj_set_size(_status_separators[i], 1, kStatusBarH - 16);
        lv_obj_set_pos(_status_separators[i], item_w * (i + 1), 8);
        lv_obj_set_style_bg_color(_status_separators[i], lv_color_hex(0x7DD3FC), 0);
        lv_obj_set_style_bg_opa(_status_separators[i], LV_OPA_50, 0);
        lv_obj_clear_flag(_status_separators[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(_status_separators[i], LV_OBJ_FLAG_CLICKABLE);
    }

    updateSystemStatusView();
    updateSsvepStatusView();
    updateLastAction(_last_action);
    updateConfValue(_conf_value);
}

lv_obj_t *SSVEPPanelUi::createStatusItem(uint8_t index, const lv_img_dsc_t *icon_src, const char *text)
{
    if ((_status_bar == nullptr) || (index >= kStatusItemCount)) {
        return nullptr;
    }

    const uint16_t item_w = kStatusBarW / kStatusItemCount;

    lv_obj_t *item = lv_obj_create(_status_bar);
    lv_obj_remove_style_all(item);
    lv_obj_set_size(item, item_w, kStatusBarH);
    lv_obj_set_pos(item, item_w * index, 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(item, LV_SCROLLBAR_MODE_OFF);

    if (index == static_cast<uint8_t>(StatusItem::SYSTEM)) {
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, systemStatusEventHandler, LV_EVENT_CLICKED, this);
    } else if (index == static_cast<uint8_t>(StatusItem::SSVEP)) {
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, ssvepStatusEventHandler, LV_EVENT_CLICKED, this);
    } else {
        lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *icon = lv_img_create(item);
    setStatusIcon(icon, icon_src);
    lv_obj_set_size(icon, kStatusIconSize, kStatusIconSize);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
    makeChildPassThrough(icon);

    lv_obj_t *label = lv_label_create(item);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, item_w - 48);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_hex(0xDFF6FF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 44, 0);
    makeChildPassThrough(label);

    _status_items[index] = item;
    _status_icons[index] = icon;
    _status_labels[index] = label;

    return item;
}

void SSVEPPanelUi::setStatusIcon(lv_obj_t *icon, const lv_img_dsc_t *icon_src)
{
    if ((icon == nullptr) || (icon_src == nullptr)) {
        return;
    }

    lv_img_set_src(icon, icon_src);
    lv_img_set_pivot(icon, 0, 0);

    const uint32_t src_w = icon_src->header.w;
    const uint32_t src_h = icon_src->header.h;
    if ((src_w > 0) && (src_h > 0)) {
        uint32_t zoom_w = (kStatusIconSize * 256U) / src_w;
        uint32_t zoom_h = (kStatusIconSize * 256U) / src_h;
        uint32_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
        if (zoom == 0) {
            zoom = 1;
        }
        lv_img_set_zoom(icon, static_cast<uint16_t>(zoom));
    }
}

void SSVEPPanelUi::startSystemWearCheck()
{
    _system_checking = true;
    setSystemStatus(SystemStatus::CHECKING);

    if (_system_check_timer != nullptr) {
        lv_timer_del(_system_check_timer);
        _system_check_timer = nullptr;
    }

    _system_check_timer = lv_timer_create(systemCheckTimerHandler, kSystemCheckMs, this);
    if (_system_check_timer != nullptr) {
        lv_timer_set_repeat_count(_system_check_timer, 1);
    }
}

bool SSVEPPanelUi::checkSystemWearStatus()
{
    return true;
}

void SSVEPPanelUi::setSystemStatus(SystemStatus status)
{
    _system_status = status;
    _system_checking = (status == SystemStatus::CHECKING);
    updateSystemStatusView();
}

void SSVEPPanelUi::updateSystemStatusView()
{
    lv_obj_t *icon = _status_icons[static_cast<uint8_t>(StatusItem::SYSTEM)];
    lv_obj_t *label = _status_labels[static_cast<uint8_t>(StatusItem::SYSTEM)];
    if ((icon == nullptr) || (label == nullptr)) {
        return;
    }

    switch (_system_status) {
    case SystemStatus::CHECKING:
        setStatusIcon(icon, &wrong_icon);
        lv_label_set_text(label, "SYSTEM test...");
        break;
    case SystemStatus::OK:
        setStatusIcon(icon, &ok_icon);
        lv_label_set_text(label, "SYSTEM OK");
        break;
    case SystemStatus::WRONG:
    default:
        setStatusIcon(icon, &wrong_icon);
        lv_label_set_text(label, "SYSTEM WRONG");
        break;
    }
}

void SSVEPPanelUi::updateSsvepStatusView()
{
    lv_obj_t *icon = _status_icons[static_cast<uint8_t>(StatusItem::SSVEP)];
    lv_obj_t *label = _status_labels[static_cast<uint8_t>(StatusItem::SSVEP)];
    if ((icon == nullptr) || (label == nullptr)) {
        return;
    }

    setStatusIcon(icon, &status_icon);
    lv_label_set_text(label, _ssvep_running ? "SSVEP RUNNING" : "SSVEP STOP");
}

void SSVEPPanelUi::notifySsvepRunningChanged()
{
    if (_ssvep_running_cb != nullptr) {
        _ssvep_running_cb(_ssvep_running, _ssvep_running_user_data);
    }
}

lv_obj_t *SSVEPPanelUi::createTile(uint8_t index, int16_t x, int16_t y)
{
    if ((_panel == nullptr) || (index >= kTileCount)) {
        return nullptr;
    }

    lv_obj_t *tile = lv_btn_create(_panel);
    lv_obj_remove_style_all(tile);

    lv_obj_set_size(tile, kTileW, kTileH);
    lv_obj_set_pos(tile, x, y);

    lv_obj_set_style_radius(tile, 20, 0);
    lv_obj_set_style_clip_corner(tile, true, 0);

    lv_obj_set_style_border_width(tile, 2, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(kTileBorderColors[index]), 0);
    lv_obj_set_style_border_opa(tile, LV_OPA_60, 0);

    lv_obj_set_style_bg_color(tile, lv_color_hex(kTileBgColors[index]), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);

    lv_obj_set_style_shadow_width(tile, 6, 0);
    lv_obj_set_style_shadow_color(tile, lv_color_hex(kTileBorderColors[index]), 0);
    lv_obj_set_style_shadow_opa(tile, LV_OPA_20, 0);

    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(tile, tileEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(tile, reinterpret_cast<void *>(static_cast<uintptr_t>(index)));

    // Frequency label: top-right.
    lv_obj_t *freq_lbl = lv_label_create(tile);
    lv_label_set_text(freq_lbl, kTileFreqs[index]);
    lv_obj_set_style_text_color(freq_lbl, lv_color_hex(kTileBorderColors[index]), 0);
    lv_obj_set_style_text_font(freq_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(freq_lbl, LV_ALIGN_TOP_RIGHT, -10, 9);
    makeChildPassThrough(freq_lbl);

    // Icon box: stable visual centering for zoomed image.
    lv_obj_t *icon_box = lv_obj_create(tile);
    lv_obj_remove_style_all(icon_box);
    lv_obj_set_size(icon_box, kIconBoxSize, kIconBoxSize);
    lv_obj_align(icon_box, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(icon_box, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_scrollbar_mode(icon_box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *icon = lv_img_create(icon_box);
    lv_img_set_src(icon, kTileImages[index]);

    const uint32_t src_w = kTileImages[index]->header.w;
    const uint32_t src_h = kTileImages[index]->header.h;
    const int32_t disp_w = static_cast<int32_t>((src_w * kIconZoom + 128) / 256);
    const int32_t disp_h = static_cast<int32_t>((src_h * kIconZoom + 128) / 256);

    lv_img_set_pivot(icon, 0, 0);
    lv_img_set_zoom(icon, kIconZoom);

    int32_t icon_x = (static_cast<int32_t>(kIconBoxSize) - disp_w) / 2;
    int32_t icon_y = (static_cast<int32_t>(kIconBoxSize) - disp_h) / 2;
    if (icon_x < 0) {
        icon_x = 0;
    }
    if (icon_y < 0) {
        icon_y = 0;
    }
    lv_obj_set_pos(icon, static_cast<lv_coord_t>(icon_x), static_cast<lv_coord_t>(icon_y));
    makeChildPassThrough(icon);

    // Title label: ASCII to avoid CJK boxes.
    lv_obj_t *lbl = lv_label_create(tile);
    lv_label_set_text(lbl, kTileTitles[index]);
    lv_obj_set_width(lbl, kTileW - 24);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xE0EAF4), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -14);
    makeChildPassThrough(lbl);

    _tiles[index]       = tile;
    _icon_boxes[index]  = icon_box;
    _icons[index]       = icon;
    _labels[index]      = lbl;
    _freq_labels[index] = freq_lbl;

    return tile;
}

void SSVEPPanelUi::notifyAction(uint8_t index)
{
    if (_action_cb != nullptr) {
        _action_cb(static_cast<int>(index), _action_user_data);
    }
}

void SSVEPPanelUi::handleTile(uint8_t index)
{
    if (index >= kTileCount) {
        return;
    }

    switch (index) {
    case 0:
        ESP_LOGI(TAG, "Call nurse triggered");
        updateLastAction(kTileTitles[0]);
        notifyAction(0);
        break;

    case 1:
        ESP_LOGI(TAG, "Room control triggered");
        updateLastAction(kTileTitles[1]);
        showRestPage();
        break;

    case 2:
        ESP_LOGI(TAG, "Life service triggered");
        updateLastAction(kTileTitles[2]);
        showLivePage();
        break;

    case 3:
        updateLastAction(kTileTitles[3]);
        if (_current_page == Page::MAIN) {
            ESP_LOGI(TAG, "Back from main");
            notifyAction(3);
        } else {
            ESP_LOGI(TAG, "Back to main page");
            showMainPage();
        }
        break;

    default:
        break;
    }
}

void SSVEPPanelUi::makeChildPassThrough(lv_obj_t *obj)
{
    if (obj == nullptr) {
        return;
    }

    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void SSVEPPanelUi::tileEventHandler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    lv_obj_t *tile = lv_event_get_current_target(e);
    SSVEPPanelUi *ui = static_cast<SSVEPPanelUi *>(lv_event_get_user_data(e));

    if ((tile == nullptr) || (ui == nullptr)) {
        return;
    }

    uintptr_t index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(tile));
    if (index >= kTileCount) {
        ESP_LOGW(TAG, "tileEventHandler: invalid index %u", static_cast<unsigned>(index));
        return;
    }

    ui->handleTile(static_cast<uint8_t>(index));
}

void SSVEPPanelUi::systemStatusEventHandler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    SSVEPPanelUi *ui = static_cast<SSVEPPanelUi *>(lv_event_get_user_data(e));
    if (ui == nullptr) {
        return;
    }

    ui->startSystemWearCheck();
}

void SSVEPPanelUi::ssvepStatusEventHandler(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    SSVEPPanelUi *ui = static_cast<SSVEPPanelUi *>(lv_event_get_user_data(e));
    if (ui == nullptr) {
        return;
    }

    ui->_ssvep_running = !ui->_ssvep_running;
    ui->updateSsvepStatusView();
    ui->notifySsvepRunningChanged();
}

void SSVEPPanelUi::systemCheckTimerHandler(lv_timer_t *timer)
{
    if (timer == nullptr) {
        return;
    }

    SSVEPPanelUi *ui = static_cast<SSVEPPanelUi *>(timer->user_data);
    if (ui == nullptr) {
        return;
    }

    ui->_system_check_timer = nullptr;
    ui->setSystemStatus(ui->checkSystemWearStatus() ? SystemStatus::OK : SystemStatus::WRONG);
}
