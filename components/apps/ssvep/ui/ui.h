/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "lvgl.h"

class SSVEPPanelUi {
public:
    enum class Page {
        MAIN,
        REST,
        LIVE
    };

    using ActionCallback = void (*)(int action, void *user_data);
    using SsvepRunningCallback = void (*)(bool running, void *user_data);

    SSVEPPanelUi();

    bool create(lv_obj_t *parent);
    void destroy();

    void showMainPage();
    void showRestPage();
    void showLivePage();
    void goBack();
    void triggerByIndex(uint8_t index);
    Page currentPage() const;

    void setActionCallback(ActionCallback cb, void *user_data);
    void setSsvepRunningCallback(SsvepRunningCallback cb, void *user_data);
    void updateLastAction(const char *action_name);
    void updateConfValue(float conf);
    void updateConfValue(const char *conf);

private:
    enum class SystemStatus {
        CHECKING,
        OK,
        WRONG
    };

    enum class StatusItem {
        SYSTEM = 0,
        SSVEP,
        LAST,
        CONF,
        COUNT
    };

    // Panel dimensions: keep the center panel compact and slightly raised.
    static constexpr uint16_t kPanelWidth  = 632;
    static constexpr uint16_t kPanelHeight = 466;

    // Tile dimensions
    static constexpr uint16_t kTileW = 286;
    static constexpr uint16_t kTileH = 190;
    static constexpr uint16_t kGapX  = 20;
    static constexpr uint16_t kGapY  = 12;

    // Tile positions inside panel
    static constexpr int16_t kLeftX   = 20;
    static constexpr int16_t kRightX  = 20 + kTileW + kGapX;
    static constexpr int16_t kTopY    = 10;
    static constexpr int16_t kBottomY = kTopY + kTileH + kGapY;

    // Icon display
    static constexpr uint16_t kIconZoom    = 240;
    static constexpr uint16_t kIconBoxSize = 128;

    static constexpr uint8_t kTileCount = 4;
    static constexpr uint8_t kStatusItemCount = 4;
    static constexpr uint16_t kStatusBarW = 600;
    static constexpr uint16_t kStatusBarH = 42;
    static constexpr int16_t kStatusBarY = 414;
    static constexpr uint16_t kStatusIconSize = 32;
    static constexpr uint32_t kSystemCheckMs = 10000;

    lv_obj_t *_parent;
    lv_obj_t *_panel;
    lv_obj_t *_tiles[kTileCount];
    lv_obj_t *_icon_boxes[kTileCount];
    lv_obj_t *_icons[kTileCount];
    lv_obj_t *_labels[kTileCount];
    lv_obj_t *_freq_labels[kTileCount];
    lv_obj_t *_status_bar;
    lv_obj_t *_status_items[kStatusItemCount];
    lv_obj_t *_status_icons[kStatusItemCount];
    lv_obj_t *_status_labels[kStatusItemCount];
    lv_obj_t *_status_separators[kStatusItemCount - 1];
    lv_timer_t *_system_check_timer;

    Page _current_page;
    Page _previous_page;
    SystemStatus _system_status;
    bool _system_checking;
    bool _ssvep_running;
    char _last_action[32];
    char _conf_value[24];
    ActionCallback _action_cb;
    void *_action_user_data;
    SsvepRunningCallback _ssvep_running_cb;
    void *_ssvep_running_user_data;

    void clearPanelContent();
    void createMainTiles();
    void createBackTileOnly();
    void createPageHeader(const char *title, const char *subtitle);
    void createStatusBar();
    void startSystemWearCheck();
    bool checkSystemWearStatus();
    void setSystemStatus(SystemStatus status);
    void updateSystemStatusView();
    void updateSsvepStatusView();
    void notifySsvepRunningChanged();

    lv_obj_t *createTile(uint8_t index, int16_t x, int16_t y);
    lv_obj_t *createStatusItem(uint8_t index, const lv_img_dsc_t *icon_src, const char *text);
    void setStatusIcon(lv_obj_t *icon, const lv_img_dsc_t *icon_src);
    void notifyAction(uint8_t index);
    void handleTile(uint8_t index);

    static void makeChildPassThrough(lv_obj_t *obj);
    static void tileEventHandler(lv_event_t *e);
    static void systemStatusEventHandler(lv_event_t *e);
    static void ssvepStatusEventHandler(lv_event_t *e);
    static void systemCheckTimerHandler(lv_timer_t *timer);
};
