/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SSVEPSpiSlaveLink.hpp"

#include <cstring>

#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

namespace {

constexpr char TAG[] = "SSVEPSpiLink";
constexpr spi_host_device_t SPI_RX_HOST = SPI3_HOST;
constexpr gpio_num_t SPI_RX_MOSI = GPIO_NUM_21;
constexpr gpio_num_t SPI_RX_MISO = GPIO_NUM_22;
constexpr gpio_num_t SPI_RX_SCLK = GPIO_NUM_23;
constexpr gpio_num_t SPI_RX_CS = GPIO_NUM_3;
constexpr TickType_t SPI_RX_TIMEOUT_TICKS = pdMS_TO_TICKS(200);

}  // namespace

SSVEPSpiSlaveLink::SSVEPSpiSlaveLink():
    _callback_ctx(nullptr),
    _callback(nullptr),
    _task_handle(nullptr),
    _task_running(false)
{
}

SSVEPSpiSlaveLink::~SSVEPSpiSlaveLink()
{
    stop();
}

bool SSVEPSpiSlaveLink::start(void *ctx, PacketCallback callback)
{
    if (_task_handle != nullptr) {
        return true;
    }
    if (callback == nullptr) {
        return false;
    }

    _callback_ctx = ctx;
    _callback = callback;
    _task_running = true;

    BaseType_t task_ret = xTaskCreatePinnedToCore(
        taskEntry,
        "ssvep_spi_rx",
        4096,
        this,
        4,
        &_task_handle,
        1
    );
    if (task_ret != pdPASS) {
        _task_running = false;
        _task_handle = nullptr;
        ESP_LOGE(TAG, "Failed to create SPI RX task");
        return false;
    }

    return true;
}

void SSVEPSpiSlaveLink::stop()
{
    _task_running = false;
    for (int i = 0; (i < 100) && (_task_handle != nullptr); ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool SSVEPSpiSlaveLink::injectTestPacket(uint8_t detected_index, uint16_t detected_hz) const
{
    if (_callback == nullptr || detected_index >= 4) {
        return false;
    }

    SpiResultPacket packet = {};
    packet.flags = 0x01;
    packet.detected_index = detected_index;
    packet.detected_hz = detected_hz;
    packet.timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
    packet.confidence = 1.0f;
    packet.correlations[detected_index] = 1.0f;
    return _callback(_callback_ctx, packet, true);
}

void SSVEPSpiSlaveLink::taskEntry(void *arg)
{
    auto *link = static_cast<SSVEPSpiSlaveLink *>(arg);
    link->taskLoop();
}

void SSVEPSpiSlaveLink::taskLoop()
{
    // 【关键修复 1】：ESP32-P4 的 DMA 强行要求 64 字节对齐
    const size_t P4_DMA_ALIGN = 64; 
    
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SPI_RX_MOSI;
    bus_cfg.miso_io_num = SPI_RX_MISO;
    bus_cfg.sclk_io_num = SPI_RX_SCLK;
    bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    bus_cfg.max_transfer_sz = P4_DMA_ALIGN; // 必须设为 64 的整数倍

    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.mode = 0;
    slave_cfg.spics_io_num = SPI_RX_CS;
    slave_cfg.queue_size = 1;

    esp_err_t err = spi_slave_initialize(SPI_RX_HOST, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_slave_initialize failed: %s", esp_err_to_name(err));
        _task_handle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "SPI slave ready on GPIO MOSI=%d MISO=%d SCLK=%d CS=%d",
             SPI_RX_MOSI, SPI_RX_MISO, SPI_RX_SCLK, SPI_RX_CS);

    // 【关键修复 2】：使用 heap_caps_aligned_alloc 强制分配绝对对齐到 64 字节地址的内存
    // 大小也必须分配 64 字节（足以装下你的 36 字节 Packet 结构体）
    void *raw_dma_buf = heap_caps_aligned_alloc(P4_DMA_ALIGN, P4_DMA_ALIGN, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (raw_dma_buf == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate 64-byte aligned DMA memory");
        spi_slave_free(SPI_RX_HOST);
        _task_handle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    // 将对齐的原始内存强转为你的结构体指针
    SpiResultPacket *rx_packet = static_cast<SpiResultPacket *>(raw_dma_buf);

    while (_task_running) {
        // 重置结构体为默认状态
        *rx_packet = SpiResultPacket{};

        spi_slave_transaction_t trans = {};
        // 【关键修复 3】：告诉 DMA 我们准备好接收 64 字节（512 位），即使主机只发 36 字节也没关系
        trans.length = P4_DMA_ALIGN * 8; 
        trans.rx_buffer = rx_packet;

        err = spi_slave_transmit(SPI_RX_HOST, &trans, SPI_RX_TIMEOUT_TICKS);
        
        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "spi_slave_transmit failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10)); // 防止底层硬件异常时死循环卡死 CPU
            continue;
        }

        // trans.trans_len 表示实际由 CS 信号框住收到的有效比特数
        // 只要收到的数据 >= 结构体的大小，就说明包收全了
        if (trans.trans_len < static_cast<int>(sizeof(SpiResultPacket) * 8)) {
            ESP_LOGW(TAG, "SPI packet too short: %d bits", (int)trans.trans_len);
            continue;
        }
        
        if (_callback != nullptr) {
            _callback(_callback_ctx, *rx_packet, false);
        }
    }

    // 释放资源
    heap_caps_free(raw_dma_buf);
    err = spi_slave_free(SPI_RX_HOST);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "spi_slave_free failed: %s", esp_err_to_name(err));
    }

    _task_handle = nullptr;
    vTaskDelete(nullptr);
}
