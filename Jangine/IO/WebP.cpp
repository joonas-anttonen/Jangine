#include "WebP.hpp"

#include <webp/demux.h>

namespace Jangine::IO
{
    WebPDecodeState::WebPDecodeState(std::span<const uint8_t> data)
    {
        copyOfOriginalData = reinterpret_cast<uint8_t *>(WebPMalloc(static_cast<size_t>(data.size())));
        decoderData = reinterpret_cast<WebPData *>(WebPMalloc(sizeof(WebPData)));
        ThrowInvalidOperationIfNull(copyOfOriginalData, "Failed to allocate memory for WebP data");
        ThrowInvalidOperationIfNull(decoderData, "Failed to allocate memory for WebPData");

        WebPData *webpData = reinterpret_cast<WebPData *>(decoderData);
        webpData->bytes = copyOfOriginalData;
        webpData->size = static_cast<size_t>(data.size());

        memcpy(copyOfOriginalData, data.data(), static_cast<size_t>(data.size()));

        WebPAnimDecoderOptions decoderOptions;
        WebPAnimDecoderOptionsInit(&decoderOptions);
        decoderOptions.color_mode = MODE_RGBA;
        decoderOptions.use_threads = 0;

        decoder = WebPAnimDecoderNew(webpData, &decoderOptions);
        ThrowInvalidOperationIfNull(decoder, "Failed to create WebPAnimDecoder from data");

        WebPAnimInfo info;
        ThrowInvalidOperationIf(WebPAnimDecoderGetInfo(decoder, &info),
                                "Failed to get WebPAnimDecoder info");

        width = info.canvas_width;
        height = info.canvas_height;
        frameCount = info.frame_count;
        loopCount = info.loop_count;
    }

    WebPDecodeState::~WebPDecodeState()
    {
        if (copyOfOriginalData)
        {
            WebPFree(copyOfOriginalData);
            copyOfOriginalData = nullptr;
        }
        if (decoderData)
        {
            WebPData *webp_data = reinterpret_cast<WebPData *>(decoderData);
            webp_data->bytes = nullptr;
            webp_data->size = 0;
        }
    }

    WebPDecodeState::Status WebPDecodeState::Next(uint8_t *out_data, std::chrono::milliseconds *out_timestamp)
    {
        if (!WebPAnimDecoderHasMoreFrames(decoder))
        {
            return Status::END_OF_STREAM;
        }

        uint8_t *out_data_ptr = nullptr;
        int32_t out_timestamp_value = 0;
        if (!WebPAnimDecoderGetNext(decoder, &out_data_ptr, &out_timestamp_value))
        {
            return Status::ERROR;
        }

        memcpy(out_data, out_data_ptr, static_cast<size_t>(width * height * 4));
        *out_timestamp = std::chrono::milliseconds(out_timestamp_value);

        return Status::OK;
    }

    void WebPDecodeState::Reset()
    {
        ThrowInvalidOperationIfNull(decoder, "WebPAnimDecoder is null");
        WebPAnimDecoderReset(decoder);
    }
}