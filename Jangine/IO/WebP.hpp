#pragma once

#include "Shared.hpp"

typedef struct WebPData WebPData;
typedef struct WebPAnimDecoder WebPAnimDecoder;

namespace Jangine::IO
{
    struct WebPDecodeState
    {
        enum class Status : uint32_t
        {
            OK = 0,
            ERROR = 1,
            END_OF_STREAM = 1,
        };

        uint32_t width;
        uint32_t height;
        uint32_t frameCount;
        uint32_t loopCount;

        uint8_t *copyOfOriginalData;
        WebPData *decoderData;
        WebPAnimDecoder *decoder;

        WebPDecodeState(std::span<const uint8_t> data);
        ~WebPDecodeState();

        Status Next(uint8_t *out_data,
                    std::chrono::milliseconds *out_timestamp);
        void Reset();
    };
}