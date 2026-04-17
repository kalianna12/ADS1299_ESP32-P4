/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "TestApp.hpp"
#include <cstring>
#include <string>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"

// 定义日志标签，用于通过串口/JTAG输出日志
static const char *TAG = "ArkTestApp";

static constexpr const char *kArkApiUrl = "https://ark.cn-beijing.volces.com/api/v3/responses";
static constexpr const char *kArkApiKey = "40c16139-0b05-469b-884e-61942a00f437";
static constexpr const char *kArkModel = "doubao-seed-2-0-pro-260215";
static constexpr const char *kTestImageUrl = "https://ark-project.tos-cn-beijing.volces.com/doc_image/ark_demo_img_1.png";
static constexpr const char *kTestPrompt = "Answer in English only. Describe this image in one short sentence.";

struct HttpResponseBuffer {
    std::string data;
};

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    auto *buffer = static_cast<HttpResponseBuffer *>(evt->user_data);
    if ((evt->event_id == HTTP_EVENT_ON_DATA) && (evt->data != nullptr) && (evt->data_len > 0) && (buffer != nullptr)) {
        buffer->data.append(static_cast<const char *>(evt->data), evt->data_len);
    }
    return ESP_OK;
}

TestApp::TestApp():
    ESP_Brookesia_PhoneApp("Test App", nullptr, true),
    _status_label(nullptr),
    _button_label(nullptr),
    _refresh_label(nullptr),
    _response_label(nullptr),
    _response_panel(nullptr),
    _spinner(nullptr),
    _request_task(nullptr),
    _ui_generation(0),
    _request_running(false)
{
}

TestApp::~TestApp()
{
}

bool TestApp::run(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x06070a), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x171a22), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

    lv_obj_t *shell = lv_obj_create(screen);
    lv_obj_set_size(shell, 920, 470);
    lv_obj_center(shell);
    lv_obj_set_style_radius(shell, 28, 0);
    lv_obj_set_style_border_width(shell, 1, 0);
    lv_obj_set_style_border_color(shell, lv_color_hex(0x353a46), 0);
    lv_obj_set_style_bg_color(shell, lv_color_hex(0x0d1016), 0);
    lv_obj_set_style_bg_grad_color(shell, lv_color_hex(0x131722), 0);
    lv_obj_set_style_bg_grad_dir(shell, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_pad_all(shell, 24, 0);
    lv_obj_clear_flag(shell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(shell);
    lv_label_set_text(title, "Doubao Ark Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf5f7fb), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *left_panel = lv_obj_create(shell);
    lv_obj_set_size(left_panel, 280, 392);
    lv_obj_align(left_panel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_radius(left_panel, 0, 0);
    lv_obj_set_style_border_width(left_panel, 0, 0);
    lv_obj_set_style_bg_opa(left_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(left_panel, 0, 0);
    lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left_title = lv_label_create(left_panel);
    lv_label_set_text(left_title, "Official Request");
    lv_obj_set_style_text_font(left_title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(left_title, lv_color_hex(0xf5f7fb), 0);
    lv_obj_align(left_title, LV_ALIGN_TOP_LEFT, 0, 8);

    // 上移 Model Label
    lv_obj_t *model_label = lv_label_create(left_panel);
    lv_obj_set_width(model_label, 250);
    lv_label_set_text_fmt(model_label, "Model\n%s", kArkModel);
    lv_obj_set_style_text_font(model_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(model_label, lv_color_hex(0xaeb6c7), 0);
    lv_obj_align(model_label, LV_ALIGN_TOP_LEFT, 0, 40); 

    // 上移 Prompt Title
    lv_obj_t *prompt_title = lv_label_create(left_panel);
    lv_label_set_text(prompt_title, "Prompt");
    lv_obj_set_style_text_font(prompt_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(prompt_title, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(prompt_title, LV_ALIGN_TOP_LEFT, 0, 90);

    // 上移并微调 Prompt Panel 高度
    lv_obj_t *prompt_panel = lv_obj_create(left_panel);
    lv_obj_set_size(prompt_panel, 250, 95); 
    lv_obj_align(prompt_panel, LV_ALIGN_TOP_LEFT, 0, 115);
    lv_obj_set_style_radius(prompt_panel, 14, 0);
    lv_obj_set_style_border_width(prompt_panel, 1, 0);
    lv_obj_set_style_border_color(prompt_panel, lv_color_hex(0x232938), 0);
    lv_obj_set_style_bg_color(prompt_panel, lv_color_hex(0x07090e), 0);
    lv_obj_set_style_pad_all(prompt_panel, 12, 0);
    lv_obj_set_scrollbar_mode(prompt_panel, LV_SCROLLBAR_MODE_ACTIVE);

    lv_obj_t *prompt_label = lv_label_create(prompt_panel);
    lv_obj_set_width(prompt_label, 220);
    lv_label_set_long_mode(prompt_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(prompt_label, kTestPrompt);
    lv_obj_set_style_text_font(prompt_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(prompt_label, lv_color_hex(0xaeb6c7), 0);
    lv_obj_align(prompt_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // 上移 Status Label
    _status_label = lv_label_create(left_panel);
    lv_label_set_text(_status_label, "Ready");
    lv_obj_set_width(_status_label, 250);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0xf2f2f2), 0);
    lv_obj_align(_status_label, LV_ALIGN_TOP_LEFT, 0, 220);

    // 上移 Request Button
    lv_obj_t *request_btn = lv_btn_create(left_panel);
    lv_obj_set_size(request_btn, 250, 54);
    lv_obj_align(request_btn, LV_ALIGN_TOP_LEFT, 0, 250);
    lv_obj_set_style_radius(request_btn, 18, 0);
    lv_obj_set_style_bg_color(request_btn, lv_color_hex(0xf3f6fb), 0);
    lv_obj_set_style_bg_grad_color(request_btn, lv_color_hex(0xdde6f7), 0);
    lv_obj_set_style_bg_grad_dir(request_btn, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_text_color(request_btn, lv_color_hex(0x10141d), 0);
    lv_obj_add_event_cb(request_btn, onButtonClicked, LV_EVENT_CLICKED, this);

    _button_label = lv_label_create(request_btn);
    lv_label_set_text(_button_label, "Run Official Test");
    lv_obj_set_style_text_font(_button_label, &lv_font_montserrat_20, 0);
    lv_obj_center(_button_label);

    // 上移 Refresh Button 解决底部被切断问题
    lv_obj_t *refresh_btn = lv_btn_create(left_panel);
    lv_obj_set_size(refresh_btn, 250, 46);
    lv_obj_align(refresh_btn, LV_ALIGN_TOP_LEFT, 0, 316);
    lv_obj_set_style_radius(refresh_btn, 16, 0);
    lv_obj_set_style_bg_opa(refresh_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(refresh_btn, 1, 0);
    lv_obj_set_style_border_color(refresh_btn, lv_color_hex(0x667089), 0);
    lv_obj_set_style_text_color(refresh_btn, lv_color_hex(0xe7edf9), 0);
    lv_obj_add_event_cb(refresh_btn, onButtonClicked, LV_EVENT_CLICKED, this);

    _refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(_refresh_label, "Refresh");
    lv_obj_set_style_text_font(_refresh_label, &lv_font_montserrat_18, 0);
    lv_obj_center(_refresh_label);

    lv_obj_t *right_panel = lv_obj_create(shell);
    lv_obj_set_size(right_panel, 560, 392);
    lv_obj_align(right_panel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_radius(right_panel, 0, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(right_panel, 0, 0);
    lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *response_title = lv_label_create(right_panel);
    lv_label_set_text(response_title, "Model Response");
    lv_obj_set_style_text_font(response_title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(response_title, lv_color_hex(0xf5f7fb), 0);
    lv_obj_align(response_title, LV_ALIGN_TOP_LEFT, 0, 8);

    _response_panel = lv_obj_create(right_panel);
    lv_obj_set_size(_response_panel, 560, 340);
    lv_obj_align(_response_panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(_response_panel, 18, 0);
    lv_obj_set_style_border_width(_response_panel, 1, 0);
    lv_obj_set_style_border_color(_response_panel, lv_color_hex(0x232938), 0);
    lv_obj_set_style_bg_color(_response_panel, lv_color_hex(0x05070c), 0);
    lv_obj_set_style_pad_all(_response_panel, 16, 0);
    lv_obj_set_scrollbar_mode(_response_panel, LV_SCROLLBAR_MODE_ACTIVE);

    _response_label = lv_label_create(_response_panel);
    lv_obj_set_width(_response_label, 520);
    lv_label_set_long_mode(_response_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(_response_label, "Press the button on the left to call the official Volcengine Ark responses API and show the returned output_text here.");
    lv_obj_set_style_text_font(_response_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(_response_label, lv_color_hex(0xd8deea), 0);
    lv_obj_align(_response_label, LV_ALIGN_TOP_LEFT, 0, 0);

    _spinner = lv_spinner_create(right_panel, 1000, 60);
    lv_obj_set_size(_spinner, 42, 42);
    lv_obj_align(_spinner, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);

    startRequest();
    return true;
}

void TestApp::onButtonClicked(lv_event_t *e)
{
    auto *self = static_cast<TestApp *>(lv_event_get_user_data(e));
    self->startRequest();
}

void TestApp::requestTaskEntry(void *arg)
{
    auto *self = static_cast<TestApp *>(arg);
    const uint32_t generation = self->_ui_generation;
    const std::string response = self->performRequest();
    
    // 【新增日志打印】将返回的完整结果通过 Log 打印到串口/JTAG 端口，方便电脑端调试
    ESP_LOGI(TAG, "========== API Response Start ==========");
    ESP_LOGI(TAG, "\n%s", response.c_str());
    ESP_LOGI(TAG, "========== API Response End   ==========");
    
    if (self->isUiValid(generation)) {
        self->setResponseText(response.c_str());
        self->setStatus("Completed");
        self->setLoading(false);
    }
    
    self->_request_running = false;
    self->_request_task = nullptr;
    vTaskDelete(nullptr);
}

void TestApp::startRequest(void)
{
    if (_request_running) {
        return;
    }
    _request_running = true;
    
    setLoading(true);
    setStatus("Requesting...");
    setResponseText("Waiting for Volcengine Ark response...");
    
    xTaskCreatePinnedToCore(requestTaskEntry, "test_app_ark", 12288, this, 4, &_request_task, tskNO_AFFINITY);
}

void TestApp::setStatus(const char *text)
{
    if (_status_label == nullptr) { return; }
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    if (_status_label != nullptr) {
        lv_label_set_text(_status_label, text);
    }
    esp_lv_adapter_unlock();
}

void TestApp::setResponseText(const char *text)
{
    if ((_response_label == nullptr) || (_response_panel == nullptr)) { return; }
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    if ((_response_label != nullptr) && (_response_panel != nullptr)) {
        lv_label_set_text(_response_label, text);
        lv_obj_scroll_to_y(_response_panel, 0, LV_ANIM_OFF);
    }
    esp_lv_adapter_unlock();
}

void TestApp::setLoading(bool loading)
{
    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    if (_spinner != nullptr) {
        if (loading) {
            lv_obj_clear_flag(_spinner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_spinner, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (_button_label != nullptr) {
        lv_label_set_text(_button_label, loading ? "Requesting..." : "Run Official Test");
    }
    if (_refresh_label != nullptr) {
        lv_label_set_text(_refresh_label, loading ? "Please wait..." : "Refresh");
    }
    esp_lv_adapter_unlock();
}

bool TestApp::isUiValid(uint32_t generation) const
{
    return (generation == _ui_generation) && (_response_label != nullptr) && (_response_panel != nullptr);
}

std::string TestApp::performRequest(void) const
{
    HttpResponseBuffer buffer;
    char auth_header[160] = {};

    // 【修改 1】将 max_output_tokens 从 60 修改为 800，给模型足够的字数把话说完
    const char *request_body = "{"
        "\"model\":\"doubao-seed-2-0-pro-260215\","
        "\"max_output_tokens\":800," 
        "\"input\":[{"
            "\"role\":\"user\","
            "\"content\":["
                "{\"type\":\"input_image\",\"image_url\":\"https://ark-project.tos-cn-beijing.volces.com/doc_image/ark_demo_img_1.png\"},"
                "{\"type\":\"input_text\",\"text\":\"Answer in English only. Describe this image in one short sentence.\"}"
            "]"
        "}]"
    "}";

    lv_snprintf(auth_header, sizeof(auth_header), "Bearer %s", kArkApiKey);

    esp_http_client_config_t config = {};
    config.url = kArkApiUrl;
    config.method = HTTP_METHOD_POST;
    config.event_handler = http_event_handler;
    config.user_data = &buffer;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 60000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return "Failed to init esp_http_client.";
    }

    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, request_body, std::strlen(request_body));

    const esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        std::string error = "HTTP request failed: ";
        error += esp_err_to_name(err);
        esp_http_client_cleanup(client);
        return error;
    }

    const int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    cJSON *root = cJSON_Parse(buffer.data.c_str());
    if (root == nullptr) {
        std::string error = "Failed to parse JSON response. Status: " + std::to_string(status_code) + "\n\n";
        error += buffer.data;
        return error;
    }

    cJSON *error_obj = cJSON_GetObjectItem(root, "error");
    if (cJSON_IsObject(error_obj)) {
        cJSON *message = cJSON_GetObjectItem(error_obj, "message");
        std::string error = "API returned error";
        if (cJSON_IsString(message) && (message->valuestring != nullptr)) {
            error += ":\n";
            error += message->valuestring;
        }
        cJSON_Delete(root);
        return error;
    }

    std::string result_text = "";
    bool found_text = false;

    // 方式 1: 标准 OpenAI/Volcengine 格式
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *choice = cJSON_GetArrayItem(choices, 0);
        cJSON *message = cJSON_GetObjectItem(choice, "message");
        if (message) {
            cJSON *content = cJSON_GetObjectItem(message, "content");
            if (cJSON_IsString(content) && content->valuestring != nullptr) {
                result_text = content->valuestring;
                found_text = true;
            }
        }
    }

    // 方式 2: 火山引擎原厂的老版嵌套 output 格式
    if (!found_text) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        if (cJSON_IsArray(output)) {
            const int count = cJSON_GetArraySize(output);
            for (int i = 0; i < count; ++i) {
                cJSON *item = cJSON_GetArrayItem(output, i);
                cJSON *type = cJSON_GetObjectItem(item, "type");
                
                if (!cJSON_IsString(type) || type->valuestring == nullptr) continue;

                // 【修改 2】兼容模型返回的 "思考过程" (reasoning)
                if (std::strcmp(type->valuestring, "reasoning") == 0) {
                    cJSON *summary = cJSON_GetObjectItem(item, "summary");
                    if (cJSON_IsArray(summary)) {
                        for (int j = 0; j < cJSON_GetArraySize(summary); ++j) {
                            cJSON *sum_item = cJSON_GetArrayItem(summary, j);
                            cJSON *sum_type = cJSON_GetObjectItem(sum_item, "type");
                            cJSON *text = cJSON_GetObjectItem(sum_item, "text");
                            if (cJSON_IsString(sum_type) && std::strcmp(sum_type->valuestring, "summary_text") == 0 && cJSON_IsString(text)) {
                                result_text += "[Reasoning (Thinking)]\n";
                                result_text += text->valuestring;
                                result_text += "\n\n";
                                found_text = true;
                            }
                        }
                    }
                } 
                // 兼容模型返回的 "最终回答" (message)
                else if (std::strcmp(type->valuestring, "message") == 0) {
                    cJSON *content = cJSON_GetObjectItem(item, "content");
                    if (cJSON_IsArray(content)) {
                        for (int j = 0; j < cJSON_GetArraySize(content); ++j) {
                            cJSON *content_item = cJSON_GetArrayItem(content, j);
                            cJSON *content_type = cJSON_GetObjectItem(content_item, "type");
                            cJSON *text = cJSON_GetObjectItem(content_item, "text");
                            if (cJSON_IsString(content_type) && std::strcmp(content_type->valuestring, "output_text") == 0 && cJSON_IsString(text)) {
                                result_text += "[Final Answer]\n";
                                result_text += text->valuestring;
                                found_text = true;
                            }
                        }
                    }
                }
            }
        } 
        else if (cJSON_IsString(output) && output->valuestring != nullptr) {
            result_text = output->valuestring;
            found_text = true;
        }
    }

    if (found_text) {
        std::string result = "Status Code: ";
        result += std::to_string(status_code);
        result += "\n\n";
        result += result_text;
        
        // 如果是因为 tokens 不够被截断的，在屏幕上提示一下
        cJSON *status = cJSON_GetObjectItem(root, "status");
        if (cJSON_IsString(status) && std::strcmp(status->valuestring, "incomplete") == 0) {
            result += "\n\n(Warning: Response was incomplete, output reached max tokens limit.)";
        }
        
        cJSON_Delete(root);
        return result;
    }

    std::string fallback = "No valid output format found in JSON. Status: " + std::to_string(status_code) + "\n\nRaw Response:\n";
    fallback += buffer.data;
    cJSON_Delete(root);
    return fallback;
}

bool TestApp::back(void)
{
    notifyCoreClosed();
    return true;
}

bool TestApp::close(void)
{
    _ui_generation++;
    _status_label = nullptr;
    _button_label = nullptr;
    _refresh_label = nullptr;
    _response_label = nullptr;
    _response_panel = nullptr;
    _spinner = nullptr;
    _request_running = false;
    return true;
}

bool TestApp::init(void)
{
    return true;
}