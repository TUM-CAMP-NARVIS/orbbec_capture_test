#ifndef ORBBEC_CAPTURE_TEST_H26XDECODER_H
#define ORBBEC_CAPTURE_TEST_H26XDECODER_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include "libavutil/imgutils.h"

#ifdef __cplusplus
}
#endif

#include <libobsensor/h/ObTypes.h>
#include <opencv2/opencv.hpp>

namespace tcn::vpf {

    class H26xDecoder {
    public:

        typedef std::function<void(cv::Mat image)> frame_handler_cb;
        H26xDecoder(frame_handler_cb cb);
        ~H26xDecoder();

    public:
        const AVCodec *codec{nullptr};
        AVCodecContext *cctx{nullptr};
        AVCodecParserContext *pCodecParserCtx{nullptr};
        int frame_count{0};
        AVFrame *frame{nullptr};
        AVPacket *avpkt{nullptr};
        AVFrame *converted_frame{nullptr};

        struct SwsContext *imgCtx{nullptr};
        bool bIsInit{false};
        int vsize{0};
    public:
        bool DecoderInit(OBFormat stream_format, OBFormat output_format);

        bool DecodeOnePacket(int cur_size, uint8_t *cur_ptr);

        OBFormat inputFormat{OB_FORMAT_UNKNOWN};
        OBFormat outputFormat{OB_FORMAT_BGR};
        AVPixelFormat frameOutputFormat{AV_PIX_FMT_NONE};

        frame_handler_cb frameCallback;
        int width{0};
        int height{0};

    };

} // vpf
// tcn

#endif //ORBBEC_CAPTURE_TEST_H26XDECODER_H
