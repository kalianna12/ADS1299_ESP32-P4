/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SpiResultProtocol.hpp"

class SSVEPSpiSlaveLink {
public:
    using PacketCallback = bool (*)(void *ctx, const SpiResultPacket &packet, bool from_test_button);

    SSVEPSpiSlaveLink();
    ~SSVEPSpiSlaveLink();

    bool start(void *ctx, PacketCallback callback);
    void stop();
    bool injectTestPacket(uint8_t detected_index, uint16_t detected_hz) const;

private:
    static void taskEntry(void *arg);
    void taskLoop();

    void *_callback_ctx;
    PacketCallback _callback;
    TaskHandle_t _task_handle;
    bool _task_running;
};
