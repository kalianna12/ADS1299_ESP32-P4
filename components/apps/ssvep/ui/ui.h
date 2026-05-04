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
    static constexpr uint16_t kTileSize = 270;
    static constexpr uint16_t kGap = 20;
    static constexpr uint16_t kPanelWidth = kTileSize * 2 + kGap;
    static constexpr uint16_t kPanelHeight = kTileSize * 2 + kGap;
    static constexpr uint8_t kTileCount = 4;

    lv_obj_t *_parent;
    lv_obj_t *_panel;
    lv_obj_t *_tiles[kTileCount];
    lv_obj_t *_icons[kTileCount];
    Page _current_page;
    Page _previous_page;
    ActionCallback _action_cb;
    void *_action_user_data;

    void clearPanelContent();
    void createMainTiles();
    void createBackTileOnly();
    lv_obj_t *createTile(uint8_t index, int16_t x, int16_t y);
    void notifyAction(uint8_t index);
    void handleTile(uint8_t index);

    static void tileEventHandler(lv_event_t *e);
};
