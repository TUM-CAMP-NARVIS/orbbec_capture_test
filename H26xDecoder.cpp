#include "H26xDecoder.h"
#include <spdlog/spdlog.h>

#include <exception>
#include <utility>

namespace tcn {
    namespace vpf {

        H26xDecoder::H26xDecoder(frame_handler_cb cb) : frameCallback(std::move(cb)) {}
        H26xDecoder::~H26xDecoder() {
            DecoderTeardown();
        }

        void H26xDecoder::DecoderTeardown() {

            // drain the codec parser
            if (cctx) {
                if (avcodec_send_packet(cctx, NULL))
                {
                    spdlog::error("avcodec_send_packet fail");
                }
                while (true) {
                    if (avcodec_receive_frame(cctx, frame) == AVERROR_EOF) {
                        break;
                    }
                }
            }

            // segfaults -- why?
//            if (pCodecParserCtx != nullptr) {
//                av_parser_close(pCodecParserCtx);
//            }

            if (cctx != nullptr) {
                avcodec_free_context(&cctx);
            }

            if (avpkt != nullptr) {
                av_packet_free(&avpkt);
            }

            if (frame != nullptr) {
                av_frame_free(&frame);
            }

            if (sw_frame != nullptr) {
                av_frame_free(&sw_frame);
            }

            if (hw_device_ctx != nullptr) {
                av_buffer_unref(&hw_device_ctx);
            }

        }

        bool H26xDecoder::DecoderInit(AVHWDeviceType device_type, OBFormat stream_format, OBFormat output_format)
        {
            inputFormat = stream_format;
            outputFormat = output_format;

            avpkt = av_packet_alloc();
            if (!avpkt) {
                spdlog::error("Could not allocate packet");
                return false;
            }

            switch (stream_format) {
                case OB_FORMAT_H264:
                    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
                    break;
                case OB_FORMAT_H265:
                case OB_FORMAT_HEVC:
                    codec = avcodec_find_decoder(AV_CODEC_ID_H265);
                    break;
                default:
                    spdlog::error("Unhandled input stream format: {0}", static_cast<int>(stream_format));
                    return false;
            }

            if (!codec) {
                spdlog::error("Codec not found.");
                return false;
            }

            if (device_type != AV_HWDEVICE_TYPE_NONE) {
                for (int i = 0;; i++) {
                    const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
                    if (!config) {
                        spdlog::error("Decoder {0} does not support device type {1}.",
                                      codec->name, av_hwdevice_get_type_name(device_type));
                        throw std::runtime_error("Error");
                    }
                    if (config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                        config->device_type == device_type) {
                        decoderOutputFormat = config->pix_fmt;
                        break;
                    }
                }
            }

            cctx = avcodec_alloc_context3(codec);
            if (!cctx) {
                spdlog::error("Could not allocate video codec context.");
                return false;
            }

            if (device_type == AV_HWDEVICE_TYPE_CUDA) {
                hwOutputFormat = AV_PIX_FMT_CUDA;
                decoderOutputFormat = AV_PIX_FMT_NV12;
            } else if (device_type != AV_HWDEVICE_TYPE_NONE) {
                spdlog::error("Unsupported hardware acceleration requested.");
                return false;
            }

            if (device_type != AV_HWDEVICE_TYPE_NONE) {
                if (av_hwdevice_ctx_create(&hw_device_ctx, device_type,
                                                  NULL, NULL, 0) < 0) {
                    spdlog::error("Failed to create specified HW device.");
                    return false;
                }
                cctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            }

            pCodecParserCtx = av_parser_init(cctx->codec_id); //初始化 AVCodecParserContext
            if (!pCodecParserCtx) {
                spdlog::error("Could not allocate video parser context.");
                return false;
            }

            if (avcodec_open2(cctx, codec, nullptr) < 0) {
                spdlog::error("Could not open codec.");
                return false;
            }

            frame = av_frame_alloc();
            if (!frame) {
                spdlog::error("Could not allocate video frame.");
                return false;
            }
            sw_frame = av_frame_alloc();
            if (!sw_frame) {
                spdlog::error("Could not allocate video sw_frame.");
                return false;
            }
            return true;
        }

        bool H26xDecoder::DecodeOnePacket(int cur_size, uint8_t *cur_ptr)
        {

            bool decodedImage{false};
            while (cur_size > 0)
            {
                unsigned char *buf = nullptr;
                int len = av_parser_parse2(
                        pCodecParserCtx, cctx,
                        &(avpkt->data), &(avpkt->size),
                        cur_ptr, cur_size,
                        AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

                // some checks ??
                cur_ptr += len;
                cur_size -= len;
                if (avpkt->size)
                {
                    if (avcodec_send_packet(cctx, avpkt))
                    {
                        spdlog::error("avcodec_send_packet fail");
                        return false;
                    }
                    if (avcodec_receive_frame(cctx, frame))
                    {
                        spdlog::error("avcodec_receive_frame fail");
                        return false;
                    }
                    AVFrame *tmp_frame{nullptr};

                    if (frame->format == hwOutputFormat) {
                        /* retrieve data from GPU to CPU */
                        int ret{0};
                        if (av_hwframe_transfer_data(sw_frame, frame, 0) < 0) {
                            spdlog::error("Error transferring the data to system memory");
                            av_frame_free(&sw_frame);
                            return false;
                        }
                        tmp_frame = sw_frame;
                    } else {
                        tmp_frame = frame;
                    }

                    if (!bIsInit)
                    {
                        width = cctx->width;
                        height = cctx->height;

                        frameOutputFormat = AV_PIX_FMT_NV12;
                        outputFormat = OB_FORMAT_NV12;
//                        switch(outputFormat) {
//                            case OB_FORMAT_BGR:
//                                frameOutputFormat = AV_PIX_FMT_BGR24;
//                                break;
//                            case OB_FORMAT_BGRA:
//                                frameOutputFormat = AV_PIX_FMT_BGRA;
//                                break;
//                            case OB_FORMAT_RGB:
//                                frameOutputFormat = AV_PIX_FMT_RGB24;
//                                break;
//                            case OB_FORMAT_NV12:
//                                frameOutputFormat = AV_PIX_FMT_NV12;
//                                break;
//                            default:
//                                spdlog::error("Unhandled output format: {0}", static_cast<int>(outputFormat));
//                                return false;
//                        }

                        // skip if in/out are identical ?
                        if (decoderOutputFormat != frameOutputFormat) {
                            imgCtx = sws_getContext(cctx->width, cctx->height, decoderOutputFormat,
                                                    cctx->width, cctx->height, frameOutputFormat,
                                                    SWS_BICUBIC, nullptr, nullptr, nullptr);

                            if (!imgCtx)
                            {
                                spdlog::error("initialization of swscale context failed.");
                                return false;
                            }

                            converted_frame = av_frame_alloc();
                            converted_frame->width = width;
                            converted_frame->height = height;
                            converted_frame->format = frameOutputFormat;
                            vsize = av_image_get_buffer_size(frameOutputFormat, cctx->width, cctx->height, 1);
                            buf = (uint8_t *)av_malloc(vsize);
                            av_image_fill_arrays(converted_frame->data, converted_frame->linesize, buf,
                                                 frameOutputFormat, cctx->width, cctx->height, 1);
                        }
                        bIsInit = true;
                    }

                    if (bIsInit)
                    {
                        cv::Mat bgr_mat;
                        if (decoderOutputFormat != AV_PIX_FMT_NV12) {
                            sws_scale(imgCtx, tmp_frame->data, tmp_frame->linesize, 0, cctx->height,
                                      converted_frame->data, converted_frame->linesize);

                            //float time = cctx->time_base.den / cctx->time_base.num;

                            cv::Mat y_mat = cv::Mat(converted_frame->height, converted_frame->width, CV_8UC1, converted_frame->data[0], converted_frame->linesize[0]);
                            cv::Mat uv_mat = cv::Mat(converted_frame->height / 2, converted_frame->width / 2, CV_8UC2, converted_frame->data[1], converted_frame->linesize[1]);
                            cv::cvtColorTwoPlane(y_mat, uv_mat, bgr_mat, cv::COLOR_YUV2BGR_NV12);
                        } else {
                            cv::Mat y_mat = cv::Mat(tmp_frame->height, tmp_frame->width, CV_8UC1, tmp_frame->data[0], tmp_frame->linesize[0]);
                            cv::Mat uv_mat = cv::Mat(tmp_frame->height / 2, tmp_frame->width / 2, CV_8UC2, tmp_frame->data[1], tmp_frame->linesize[1]);
                            cv::cvtColorTwoPlane(y_mat, uv_mat, bgr_mat, cv::COLOR_YUV2BGR_NV12);
                        }

                        frameCallback(bgr_mat);
                        decodedImage = true;
                    }
                }
            }
            if (!decodedImage) {
                spdlog::warn("no image decoded...");
            }
            return true;
        }

    } // vpf
} // tcn