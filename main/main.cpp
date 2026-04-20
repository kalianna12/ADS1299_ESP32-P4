/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_spiffs.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "esp_lv_adapter.h"
#include "lvgl_adapter_init.h"
#include "esp_ldo_regulator.h"

#include "esp_brookesia.hpp"
#include "apps.h"

static const char *TAG = "main";
static esp_ldo_channel_handle_t sd_ldo_handle = NULL;

namespace {

struct AppRegistryEntry {
    const char *name;
    int id;
};

static ESP_Brookesia_Phone *s_phone = nullptr;
static AppRegistryEntry s_apps[16] = {};
static size_t s_app_count = 0;
static constexpr uint32_t STATUS_POLL_PERIOD_MS = 200;
static constexpr size_t LOW_INTERNAL_HEAP_THRESHOLD = 120 * 1024;
static constexpr size_t LOW_SPIRAM_HEAP_THRESHOLD = 512 * 1024;
static bool s_rom_note_printed = false;
static int s_last_active_app_id = -2;
static size_t s_last_internal_free = 0;
static size_t s_last_spiram_free = 0;

#define ENABLE_AUTO_APP_STRESS  0
#define AUTO_STRESS_ROUND_ROBIN 0
#define AUTO_STRESS_APP_NAME    "camera"
#define AUTO_STRESS_START_DELAY_MS 5000
#define AUTO_STRESS_STAY_MS     3000
#define AUTO_STRESS_BACK_WAIT_MS 2000
#define AUTO_STRESS_LOOP_DELAY_MS 2000
#define ENABLE_CUSTOM_TEST_APP  1
#define ENABLE_CUSTOM_SSVEP_APP 1

static void register_app(const char *name, int id)
{
    if ((name == nullptr) || (id < 0) || (s_app_count >= (sizeof(s_apps) / sizeof(s_apps[0])))) {
        return;
    }
    s_apps[s_app_count++] = {.name = name, .id = id};
}

static const AppRegistryEntry *find_app_by_name(const char *name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < s_app_count; ++i) {
        if ((s_apps[i].name != nullptr) && (std::strcmp(s_apps[i].name, name) == 0)) {
            return &s_apps[i];
        }
    }
    return nullptr;
}

static bool lock_phone(void)
{
    return (s_phone != nullptr) && (esp_lv_adapter_lock(-1) == ESP_OK);
}

static void unlock_phone(void)
{
    if (s_phone != nullptr) {
        esp_lv_adapter_unlock();
    }
}

static bool start_app_by_id(int id)
{
    if (!lock_phone()) {
        ESP_LOGE(TAG, "Failed to lock LVGL for start app");
        return false;
    }

    ESP_Brookesia_CoreAppEventData_t event = {
        .id = id,
        .type = ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START,
        .data = nullptr,
    };
    bool ok = s_phone->sendAppEvent(&event);
    unlock_phone();
    return ok;
}

static bool navigate_back(void)
{
    if (!lock_phone()) {
        ESP_LOGE(TAG, "Failed to lock LVGL for back navigation");
        return false;
    }

    bool ok = s_phone->sendNavigateEvent(ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK);
    unlock_phone();
    return ok;
}

static void print_heap_line(const char *name, uint32_t caps)
{
    multi_heap_info_t info = {};
    heap_caps_get_info(&info, caps);
    ESP_LOGI(TAG,
             "[mem] %s free=%u largest=%u min=%u alloc_blocks=%u free_blocks=%u",
             name,
             static_cast<unsigned>(info.total_free_bytes),
             static_cast<unsigned>(info.largest_free_block),
             static_cast<unsigned>(info.minimum_free_bytes),
             static_cast<unsigned>(info.allocated_blocks),
             static_cast<unsigned>(info.free_blocks));
}

static void print_memory_stats(void)
{
    uint32_t flash_size = 0;
    size_t spiffs_total = 0;
    size_t spiffs_used = 0;
    esp_err_t flash_ret = esp_flash_get_size(nullptr, &flash_size);
    esp_err_t spiffs_ret = esp_spiffs_info("storage", &spiffs_total, &spiffs_used);

    print_heap_line("sram", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    print_heap_line("psram", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (flash_ret == ESP_OK) {
        ESP_LOGI(TAG,
                 "[mem] flash chip total=%u bytes (%.2f MB)",
                 static_cast<unsigned>(flash_size),
                 static_cast<double>(flash_size) / (1024.0 * 1024.0));
    } else {
        ESP_LOGW(TAG, "[mem] flash chip total query failed: %s", esp_err_to_name(flash_ret));
    }

    if (spiffs_ret == ESP_OK) {
        const size_t spiffs_free = (spiffs_total >= spiffs_used) ? (spiffs_total - spiffs_used) : 0;
        ESP_LOGI(TAG,
                 "[mem] flash spiffs total=%u used=%u free=%u",
                 static_cast<unsigned>(spiffs_total),
                 static_cast<unsigned>(spiffs_used),
                 static_cast<unsigned>(spiffs_free));
    } else {
        ESP_LOGW(TAG, "[mem] flash spiffs info query failed: %s", esp_err_to_name(spiffs_ret));
    }

    if (!s_rom_note_printed) {
        ESP_LOGI(TAG, "[mem] rom is read-only firmware space, no runtime free metric");
        s_rom_note_printed = true;
    }
}

static void print_active_app(void)
{
    if (!lock_phone()) {
        ESP_LOGE(TAG, "Failed to lock LVGL for active app query");
        return;
    }

    ESP_Brookesia_CoreApp *active = s_phone->getManager().getActiveApp();
    if (active == nullptr) {
        ESP_LOGI(TAG, "No active app");
    } else {
        ESP_LOGI(TAG, "Active app: id=%d name=%s", active->getId(), active->getName());
    }

    unlock_phone();
}

static void print_app_transition_if_needed(const char *active_name, int active_id, size_t internal_free, size_t spiram_free)
{
    if (s_last_active_app_id == -2) {
        s_last_active_app_id = active_id;
        s_last_internal_free = internal_free;
        s_last_spiram_free = spiram_free;
        ESP_LOGI(TAG, "[status] active=%s(id=%d)", active_name, active_id);
        print_memory_stats();
        return;
    }

    if (active_id == s_last_active_app_id) {
        return;
    }

    ESP_LOGW(TAG,
             "[transition] active=%s(id=%d) sram_delta=%d psram_delta=%d",
             active_name,
             active_id,
             static_cast<int>(internal_free) - static_cast<int>(s_last_internal_free),
             static_cast<int>(spiram_free) - static_cast<int>(s_last_spiram_free));
    ESP_LOGI(TAG, "[status] active=%s(id=%d)", active_name, active_id);
    print_memory_stats();

    s_last_active_app_id = active_id;
    s_last_internal_free = internal_free;
    s_last_spiram_free = spiram_free;
}

static void status_monitor_task(void *arg)
{
    (void)arg;
    size_t internal_free = 0;
    size_t spiram_free = 0;
    int active_id = -1;
    const char *active_name = "home";

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Background status monitor started, event-driven print enabled");

    while (true) {
        internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (lock_phone()) {
            ESP_Brookesia_CoreManager &manager = s_phone->getManager();
            ESP_Brookesia_CoreApp *active = manager.getActiveApp();
            if (active == nullptr) {
                active_id = -1;
                active_name = "home";
                print_app_transition_if_needed(active_name, active_id, internal_free, spiram_free);
            } else {
                active_id = active->getId();
                active_name = active->getName();
                print_app_transition_if_needed(active_name, active_id, internal_free, spiram_free);
            }
            unlock_phone();
        } else {
            ESP_LOGW(TAG, "[status] failed to lock LVGL, skip active app query");
        }
        if ((internal_free < LOW_INTERNAL_HEAP_THRESHOLD) || (spiram_free < LOW_SPIRAM_HEAP_THRESHOLD)) {
            ESP_LOGW(TAG,
                     "[status] low memory sram=%u psram=%u",
                     static_cast<unsigned>(internal_free),
                     static_cast<unsigned>(spiram_free));
            print_memory_stats();
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_POLL_PERIOD_MS));
    }
}

static void auto_app_stress_task(void *arg)
{
    (void)arg;
    const AppRegistryEntry *target_app = nullptr;
    size_t target_index = 0;

    vTaskDelay(pdMS_TO_TICKS(AUTO_STRESS_START_DELAY_MS));
    if (s_app_count == 0) {
        ESP_LOGE(TAG, "[auto_stress] no installed apps");
        vTaskDelete(nullptr);
        return;
    }

#if AUTO_STRESS_ROUND_ROBIN
    ESP_LOGW(TAG,
             "[auto_stress] enabled round-robin stay_ms=%u back_wait_ms=%u loop_delay_ms=%u app_count=%u",
             AUTO_STRESS_STAY_MS,
             AUTO_STRESS_BACK_WAIT_MS,
             AUTO_STRESS_LOOP_DELAY_MS,
             static_cast<unsigned>(s_app_count));
#else
    target_app = find_app_by_name(AUTO_STRESS_APP_NAME);
    if (target_app == nullptr) {
        ESP_LOGE(TAG, "[auto_stress] target app not found: %s", AUTO_STRESS_APP_NAME);
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGW(TAG,
             "[auto_stress] enabled target=%s id=%d stay_ms=%u back_wait_ms=%u loop_delay_ms=%u",
             target_app->name,
             target_app->id,
             AUTO_STRESS_STAY_MS,
             AUTO_STRESS_BACK_WAIT_MS,
             AUTO_STRESS_LOOP_DELAY_MS);
#endif

    while (true) {
#if AUTO_STRESS_ROUND_ROBIN
        target_app = &s_apps[target_index];
        target_index = (target_index + 1) % s_app_count;
#endif
        ESP_LOGW(TAG, "[auto_stress] opening %s(id=%d)", target_app->name, target_app->id);
        if (!start_app_by_id(target_app->id)) {
            ESP_LOGE(TAG, "[auto_stress] failed to open %s", target_app->name);
        }
        vTaskDelay(pdMS_TO_TICKS(AUTO_STRESS_STAY_MS));

        ESP_LOGW(TAG, "[auto_stress] backing from %s(id=%d)", target_app->name, target_app->id);
        if (!navigate_back()) {
            ESP_LOGE(TAG, "[auto_stress] failed to navigate back");
        }
        vTaskDelay(pdMS_TO_TICKS(AUTO_STRESS_BACK_WAIT_MS));
        vTaskDelay(pdMS_TO_TICKS(AUTO_STRESS_LOOP_DELAY_MS));
    }
}

}  // namespace

static esp_err_t init_sd_ldo_only(void)
{
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };
    return esp_ldo_acquire_channel(&ldo_cfg, &sd_ldo_handle);
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(bsp_spiffs_mount());
    ESP_LOGI(TAG, "SPIFFS mount successfully");

#if CONFIG_EXAMPLE_ENABLE_SD_CARD
    ESP_ERROR_CHECK(bsp_sdcard_mount());
    ESP_LOGI(TAG, "SD card mount successfully");
#else
    ESP_ERROR_CHECK(init_sd_ldo_only());
#endif

    ESP_ERROR_CHECK(bsp_extra_codec_init());

    bsp_display_cfg_t cfg = {
        .hw_cfg = {
            .hdmi_resolution = BSP_HDMI_RES_NONE,
            .dsi_bus = {
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
            },
        },
    };
    lv_display_t *disp = lvgl_adapter_init(&cfg);
    assert(disp != nullptr && "Failed to init LVGL adapter");
    bsp_display_backlight_on();

    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));

    s_phone = new ESP_Brookesia_Phone();
    assert(s_phone != nullptr && "Failed to create phone");

    ESP_Brookesia_PhoneStylesheet_t *phone_stylesheet =
        new ESP_Brookesia_PhoneStylesheet_t ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET();
    ESP_BROOKESIA_CHECK_NULL_EXIT(phone_stylesheet, "Create phone stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(s_phone->addStylesheet(*phone_stylesheet), "Add phone stylesheet failed");
    ESP_BROOKESIA_CHECK_FALSE_EXIT(s_phone->activateStylesheet(*phone_stylesheet), "Activate phone stylesheet failed");
    assert(s_phone->begin() && "Failed to begin phone");

    PhoneAppSquareline *smart_gadget = new PhoneAppSquareline();
    assert(smart_gadget != nullptr && "Failed to create phone app squareline");
    register_app("squareline", s_phone->installApp(smart_gadget));

    Calculator *calculator = new Calculator();
    assert(calculator != nullptr && "Failed to create calculator");
    register_app("calculator", s_phone->installApp(calculator));

    MusicPlayer *music_player = new MusicPlayer();
    assert(music_player != nullptr && "Failed to create music_player");
    register_app("music", s_phone->installApp(music_player));

    AppSettings *app_settings = new AppSettings();
    assert(app_settings != nullptr && "Failed to create app_settings");
    register_app("settings", s_phone->installApp(app_settings));

    Game2048 *game_2048 = new Game2048();
    assert(game_2048 != nullptr && "Failed to create game_2048");
    register_app("2048", s_phone->installApp(game_2048));

    Camera *camera = new Camera(1280, 720);
    assert(camera != nullptr && "Failed to create camera");
    register_app("camera", s_phone->installApp(camera));

#if ENABLE_CUSTOM_TEST_APP
    TestApp *test_app = new TestApp();
    assert(test_app != nullptr && "Failed to create test_app");
    register_app("test", s_phone->installApp(test_app));
#endif

#if ENABLE_CUSTOM_SSVEP_APP
    SSVEPApp *ssvep_app = new SSVEPApp();
    assert(ssvep_app != nullptr && "Failed to create ssvep_app");
    register_app("ssvep", s_phone->installApp(ssvep_app));
#endif

#if CONFIG_EXAMPLE_ENABLE_SD_CARD
    ESP_LOGW(TAG, "Using Video Player example requires inserting the SD card in advance and saving an MJPEG format video on the SD card");
    AppVideoPlayer *app_video_player = new AppVideoPlayer();
    assert(app_video_player != nullptr && "Failed to create app_video_player");
    register_app("video", s_phone->installApp(app_video_player));
#endif

    for (size_t i = 0; i < s_app_count; ++i) {
        assert(s_apps[i].id >= 0 && "Failed to install app");
    }

    esp_lv_adapter_unlock();

    xTaskCreate(status_monitor_task, "status_monitor", 4096, nullptr, 2, nullptr);
#if ENABLE_AUTO_APP_STRESS
    xTaskCreate(auto_app_stress_task, "auto_app_stress", 4096, nullptr, 2, nullptr);
#endif
}
