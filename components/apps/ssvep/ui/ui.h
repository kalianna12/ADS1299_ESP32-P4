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

private:
    // Panel dimensions: slightly smaller than previous 650x500
    static constexpr uint16_t kPanelWidth  = 640;
    static constexpr uint16_t kPanelHeight = 480;

    // Tile dimensions
    static constexpr uint16_t kTileW = 295;
    static constexpr uint16_t kTileH = 205;
    static constexpr uint16_t kGapX  = 20;
    static constexpr uint16_t kGapY  = 20;

    // Tile positions inside panel
    static constexpr int16_t kLeftX   = 15;
    static constexpr int16_t kRightX  = 15 + kTileW + kGapX;   // 330
    static constexpr int16_t kTopY    = 20;
    static constexpr int16_t kBottomY = 20 + kTileH + kGapY;   // 245

    // Icon display
    static constexpr uint16_t kIconZoom    = 280;
    static constexpr uint16_t kIconBoxSize = 150;

    static constexpr uint8_t kTileCount = 4;

    lv_obj_t *_parent;
    lv_obj_t *_panel;
    lv_obj_t *_tiles[kTileCount];
    lv_obj_t *_icon_boxes[kTileCount];
    lv_obj_t *_icons[kTileCount];
    lv_obj_t *_labels[kTileCount];
    lv_obj_t *_freq_labels[kTileCount];

    Page _current_page;
    Page _previous_page;
    ActionCallback _action_cb;
    void *_action_user_data;

    void clearPanelContent();
    void createMainTiles();
    void createBackTileOnly();
    void createPageHeader(const char *title, const char *subtitle);

    lv_obj_t *createTile(uint8_t index, int16_t x, int16_t y);
    void notifyAction(uint8_t index);
    void handleTile(uint8_t index);

    static void makeChildPassThrough(lv_obj_t *obj);
    static void tileEventHandler(lv_event_t *e);
};
