#include "H26xDecoder.h"
#include <spdlog/spdlog.h>

#include <exception>
#include <utility>

namespace tcn {
    namespace vpf {
        H26xDecoder::H26xDecoder(frame_handler_cb cb) : frameCallback(std::move(cb)) {}
        H26xDecoder::~H26xDecoder() = default;

        bool H26xDecoder::DecoderInit(OBFormat stream_format, OBFormat output_format)
        {
            inputFormat = stream_format;
            outputFormat = output_format;

            avpkt = av_packet_alloc();
            if (!avpkt) {
                spdlog::error("Could not allocate packet");
                exit(1);
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

            cctx = avcodec_alloc_context3(codec);
            if (!cctx) {
                spdlog::error("Could not allocate video codec context.");
                return false;
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
                        imgCtx = sws_getContext(cctx->width, cctx->height, cctx->pix_fmt,
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
                        vsize = av_image_get_buffer_size(frameOutputFormat, cctx->width, cctx->height, 1);
                        buf = (uint8_t *)av_malloc(vsize);
                        av_image_fill_arrays(converted_frame->data, converted_frame->linesize, buf,
                                             frameOutputFormat, cctx->width, cctx->height, 1);
                        bIsInit = true;
                    }

                    if (bIsInit)
                    {
                        sws_scale(imgCtx, frame->data, frame->linesize, 0, cctx->height,
                                  converted_frame->data, converted_frame->linesize);

                        //float time = cctx->time_base.den / cctx->time_base.num;

                        cv::Mat y_mat = cv::Mat(converted_frame->height, converted_frame->width, CV_8UC1, converted_frame->data[0], converted_frame->linesize[0]);
                        cv::Mat uv_mat = cv::Mat(converted_frame->height / 2, converted_frame->width / 2, CV_8UC2, converted_frame->data[1], converted_frame->linesize[1]);
                        cv::Mat bgr_mat;
                        cv::cvtColorTwoPlane(y_mat, uv_mat, bgr_mat, cv::COLOR_YUV2BGR_NV12);

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