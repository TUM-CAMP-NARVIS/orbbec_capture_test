#pragma once


#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}



static std::string AvErrorToString(int av_error_code)
{
    const auto buf_size = 1024U;
    char* err_string = (char*)calloc(buf_size, sizeof(*err_string));
    if (!err_string) {
        return {};
    }

    if (0 != av_strerror(av_error_code, err_string, buf_size - 1)) {
        free(err_string);
        std::stringstream ss;
        ss << "Unknown error with code " << av_error_code;
        return ss.str();
    }

    std::string str(err_string);
    free(err_string);
    return str;
}

namespace capture_test {

    struct H26xDecoder {

        AVBufferRef *hw_device_ctx = NULL;
        AVPixelFormat hw_pix_fmt;

        int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
        {
            int err = 0;

            if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                              NULL, NULL, 0)) < 0) {
                fprintf(stderr, "Failed to create specified HW device.\n");
                return err;
            }
            ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

            return err;
        }

        AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                    const AVPixelFormat *pix_fmts)
        {
            const AVPixelFormat *p;

            for (p = pix_fmts; *p != -1; p++) {
                if (*p == hw_pix_fmt)
                    return *p;
            }

            fprintf(stderr, "Failed to get HW surface format.\n");
            return AV_PIX_FMT_NONE;
        }

        int decode_write(AVCodecContext *avctx, AVPacket *packet)
        {
            AVFrame *frame = NULL, *sw_frame = NULL;
            AVFrame *tmp_frame = NULL;
            uint8_t *buffer = NULL;
            int size;
            int ret = 0;

            ret = avcodec_send_packet(avctx, packet);
            if (ret < 0) {
                fprintf(stderr, "Error during decoding\n");
                return ret;
            }

            while (1) {
                if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
                    fprintf(stderr, "Can not alloc frame\n");
                    ret = AVERROR(ENOMEM);
                    goto fail;
                }

                ret = avcodec_receive_frame(avctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_frame_free(&frame);
                    av_frame_free(&sw_frame);
                    return 0;
                } else if (ret < 0) {
                    fprintf(stderr, "Error while decoding\n");
                    goto fail;
                }

                if (frame->format == hw_pix_fmt) {
                    /* retrieve data from GPU to CPU */
                    if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                        fprintf(stderr, "Error transferring the data to system memory\n");
                        goto fail;
                    }
                    tmp_frame = sw_frame;
                } else
                    tmp_frame = frame;

                size = av_image_get_buffer_size(static_cast<AVPixelFormat>(tmp_frame->format),
                                                tmp_frame->width,
                                                tmp_frame->height, 1);
                buffer = static_cast<uint8_t*>(av_malloc(size));
                if (!buffer) {
                    fprintf(stderr, "Can not alloc buffer\n");
                    ret = AVERROR(ENOMEM);
                    goto fail;
                }
                ret = av_image_copy_to_buffer(buffer, size,
                                              (const uint8_t * const *)tmp_frame->data,
                                              (const int *)tmp_frame->linesize,
                                              static_cast<AVPixelFormat>(tmp_frame->format),
                                              tmp_frame->width, tmp_frame->height, 1);
                if (ret < 0) {
                    fprintf(stderr, "Can not copy image to buffer\n");
                    goto fail;
                }


                // do something with buffer/size
//                if ((ret = fwrite(buffer, 1, size, output_file)) < 0) {
//                    fprintf(stderr, "Failed to dump raw data.\n");
//                    goto fail;
//                }

                fail:
                av_frame_free(&frame);
                av_frame_free(&sw_frame);
                av_freep(&buffer);
                if (ret < 0)
                    return ret;
            }
        }

    };
} // end ns