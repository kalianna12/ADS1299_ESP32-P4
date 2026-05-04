#include "ui.h"

#include "esp_log.h"

extern "C" {
    extern const lv_img_dsc_t call_icon;
    extern const lv_img_dsc_t rest_icon;
    extern const lv_img_dsc_t live_icon;
    extern const lv_img_dsc_t back_icon;
}

static const char *TAG = "SSVEPPanelUi";

static const lv_img_dsc_t *const kTileImages[] = {
    &call_icon,
    &rest_icon,
    &live_icon,
    &back_icon
};

static constexpr uint16_t kIconZoom = 180;

SSVEPPanelUi::SSVEPPanelUi():
    _parent(nullptr),
    _panel(nullptr),
    _tiles{nullptr, nullptr, nullptr, nullptr},
    _icons{nullptr, nullptr, nullptr, nullptr},
    _current_page(Page::MAIN),
    _previous_page(Page::MAIN),
    _action_cb(nullptr),
    _action_user_data(nullptr)
{
}

bool SSVEPPanelUi::create(lv_obj_t *parent)
{
    if (parent == nullptr) {
        return false;
    }

    destroy();

    _parent = parent;
    _panel = lv_obj_create(parent);
    lv_obj_set_size(_panel, kPanelWidth, kPanelHeight);
    lv_obj_center(_panel);
    lv_obj_set_style_radius(_panel, 0, 0);
    lv_obj_set_style_border_width(_panel, 0, 0);
    lv_obj_set_style_bg_opa(_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(_panel, 0, 0);
    lv_obj_clear_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);

    showMainPage();
    return true;
}

void SSVEPPanelUi::destroy()
{
    if (_panel != nullptr) {
        lv_obj_del(_panel);
    }

    _parent = nullptr;
    _panel = nullptr;
    for (uint8_t i = 0; i < kTileCount; i++) {
        _tiles[i] = nullptr;
        _icons[i] = nullptr;
    }
    _current_page = Page::MAIN;
    _previous_page = Page::MAIN;
}

void SSVEPPanelUi::showMainPage()
{
    if (_panel == nullptr) {
        return;
    }

    _previous_page = _current_page;
    _current_page = Page::MAIN;
    clearPanelContent();
    createMainTiles();
}

void SSVEPPanelUi::showRestPage()
{
    if (_panel == nullptr) {
        return;
    }

    _previous_page = _current_page;
    _current_page = Page::REST;
    clearPanelContent();
    createBackTileOnly();
}

void SSVEPPanelUi::showLivePage()
{
    if (_panel == nullptr) {
        return;
    }

    _previous_page = _current_page;
    _current_page = Page::LIVE;
    clearPanelContent();
    createBackTileOnly();
}

void SSVEPPanelUi::goBack()
{
    if (_panel == nullptr) {
        return;
    }

    if (_current_page == Page::MAIN) {
        showMainPage();
        return;
    }

    showMainPage();
}

void SSVEPPanelUi::triggerByIndex(uint8_t index)
{
    handleTile(index);
}

SSVEPPanelUi::Page SSVEPPanelUi::currentPage() const
{
    return _current_page;
}

void SSVEPPanelUi::setActionCallback(ActionCallback cb, void *user_data)
{
    _action_cb = cb;
    _action_user_data = user_data;
}

void SSVEPPanelUi::clearPanelContent()
{
    if (_panel == nullptr) {
        return;
    }

    lv_obj_clean(_panel);
    for (uint8_t i = 0; i < kTileCount; i++) {
        _tiles[i] = nullptr;
        _icons[i] = nullptr;
    }
}

void SSVEPPanelUi::createMainTiles()
{
    createTile(0, 0, 0);
    createTile(1, kTileSize + kGap, 0);
    createTile(2, 0, kTileSize + kGap);
    createTile(3, kTileSize + kGap, kTileSize + kGap);
}

void SSVEPPanelUi::createBackTileOnly()
{
    createTile(3, kTileSize + kGap, kTileSize + kGap);
}

lv_obj_t *SSVEPPanelUi::createTile(uint8_t index, int16_t x, int16_t y)
{
    if ((_panel == nullptr) || (index >= kTileCount)) {
        return nullptr;
    }

    lv_obj_t *tile = lv_btn_create(_panel);
    lv_obj_set_size(tile, kTileSize, kTileSize);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_radius(tile, 8, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_shadow_width(tile, 0, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tile, tileEventHandler, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(tile, reinterpret_cast<void *>(static_cast<uintptr_t>(index)));

    lv_obj_t *icon = lv_img_create(tile);
    lv_img_set_src(icon, kTileImages[index]);
    lv_img_set_zoom(icon, kIconZoom);
    lv_obj_center(icon);

    _tiles[index] = tile;
    _icons[index] = icon;
    return tile;
}

void SSVEPPanelUi::notifyAction(uint8_t index)
{
    if (_action_cb != nullptr) {
        _action_cb(index, _action_user_data);
    }
}

void SSVEPPanelUi::handleTile(uint8_t index)
{
    if ((_panel == nullptr) || (index >= kTileCount)) {
        return;
    }

    notifyAction(index);

    switch (index) {
    case 0:
        ESP_LOGI(TAG, "Call nurse triggered");
        break;
    case 1:
        showRestPage();
        break;
    case 2:
        showLivePage();
        break;
    case 3:
        goBack();
        break;
    default:
        break;
    }
}

void SSVEPPanelUi::tileEventHandler(lv_event_t *e)
{
    lv_obj_t *tile = lv_event_get_target(e);
    SSVEPPanelUi *ui = static_cast<SSVEPPanelUi *>(lv_event_get_user_data(e));
    if ((tile == nullptr) || (ui == nullptr)) {
        return;
    }

    uintptr_t index = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(tile));
    ui->handleTile(static_cast<uint8_t>(index));
}
