/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>

static constexpr uint16_t SPI_RESULT_PACKET_MAGIC = 0x5353;
static constexpr uint8_t SPI_RESULT_PACKET_VERSION = 1;

struct __attribute__((packed)) SpiResultPacket {
    uint16_t magic = SPI_RESULT_PACKET_MAGIC;
    uint8_t version = SPI_RESULT_PACKET_VERSION;
    uint8_t flags = 0;
    uint8_t detected_index = 0xFF;
    uint8_t reserved0 = 0;
    uint16_t detected_hz = 0;
    uint16_t binary_seq = 0;
    uint16_t reserved1 = 0;
    uint32_t sample_counter = 0;
    uint32_t timestamp_ms = 0;
    float confidence = 0.0f;
    float correlations[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};
