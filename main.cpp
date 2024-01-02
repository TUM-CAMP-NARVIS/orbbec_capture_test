#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"

#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>
#include <sstream>
#include <string>
#include <optional>
#include <future>
#include <spdlog/spdlog.h>

#include "h264nal/h264_bitstream_parser.h"
#include "h264nal/h264_common.h"

#include "pcpd_codec_nvenc/stream_decoder.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda/common.hpp>
#include <opencv2/core/cuda_stream_accessor.hpp>

#include "pcpd_cuda_core/core.cuh"
#include "pcpd_core/dataflow/buffered_channel.h"

std::unique_ptr<pcpd::codec_nvenc::StreamDecoder> decoder;
cv::cuda::GpuMat output_image;
size_t image_size_bytes{0};
std::shared_ptr<ob::FrameSet> currentFrameSet;
std::mutex                    frameSetMutex;
uint64_t frameCounter{0};
std::optional<cudaStream_t> cuda_stream;

struct FrameInfo{
    uint64_t frame_idx{0};
    cv::Mat image;
};

pcpd::dataflow::buffered_channel<FrameInfo> frame_queue{32};
std::atomic<bool> should_stop{false};

int main(int argc, char **argv) try {

    checkCudaErrors(cudaSetDevice(0));

    // Create a Context
    ob::Context ctx;

    // Enter the device ip address (currently only FemtoMega devices support network connection, and its default ip address is 192.168.1.10)
    std::string ip;
    std::cout << "Input your device ip(default: 10.0.60.40):";
    std::getline(std::cin, ip);
    if(ip.empty()) {
        ip = "10.0.60.40";
    }


    auto receiver = std::async([&]() {
        spdlog::info("start receiver thread");
        while (!should_stop) {
            FrameInfo info{};
            auto ret = frame_queue.pop_wait_for(info, std::chrono::milliseconds(5));
            if (ret == pcpd::dataflow::channel_op_status::timeout) {
                continue;
            } else if (ret == pcpd::dataflow::channel_op_status::success) {
                spdlog::info("got next_frame; {0}", info.frame_idx);
                cv::Mat image;
                cv::cvtColor(info.image, image, cv::COLOR_RGBA2BGR);
                cv::imshow("color", image);
                cv::waitKey(5);
            } else {
                spdlog::warn("unexpect buffer_channel return status.");
            }
        }
        spdlog::info("finish receiver thread");
    });


    // Create a network device through ip (the default port number is: 8090, devices that currently support network mode do not support modifying the port
    // number)
    auto device = ctx.createNetDevice(ip.c_str(), 8090);

    // pass in device to create pipeline
    auto pipe = std::make_shared<ob::Pipeline>(device);

    // Create Config for configuring Pipeline work
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    // Get the color camera configuration list
    auto colorProfileList = pipe->getStreamProfileList(OB_SENSOR_COLOR);
    // use default configuration
    auto colorProfile = colorProfileList->getProfile(OB_PROFILE_DEFAULT);
    // enable depth stream

    config->enableStream(colorProfile);

    auto cb = [&](std::shared_ptr<ob::FrameSet> fs) {
        if (!fs) {
            spdlog::error("received invalid frameset");
            return;
        }

        auto cf = fs->colorFrame();
        auto ts = cf->timeStamp();
        auto idx = cf->index();

        OBFormat fmt = colorProfile->format();
        if (cf && fmt == OB_FORMAT_H264) {

            if (!cuda_stream.has_value()) {
                cuda_stream = cudaStream_t{};
                cudaStreamCreate ( &cuda_stream.value() );
            }
            if (!decoder) {
                decoder = std::make_unique<pcpd::codec_nvenc::StreamDecoder>("testdecoder");
                if (!decoder->initialize(pcpd::datatypes::BitstreamEncodingType::H264,
                                         pcpd::datatypes::PixelFormatType::RGBA,
                                         cf->width(), cf->height(), 32)) {
                    spdlog::error("error initializing decoder.");
                    return;
                }
                output_image = cv::cuda::GpuMat(cv::Size(cf->width(), cf->height()), CV_8UC4);
                image_size_bytes = cf->width() * cf->height() * 4;
                spdlog::info("created output buffer: {0}x{1} ({2})", cf->width(), cf->height(), image_size_bytes);
            }

//            std::size_t current_head{0};
//            auto data = static_cast<uint8_t*>(cf->data());
//            auto nalu_indices =
//                    h264nal::H264BitstreamParser::FindNaluIndices(data, cf->dataSize());
//            h264nal::H264BitstreamParserState bitstream_parser_state;
//            h264nal::ParsingOptions parsing_options;
//            auto bitstream =
//                    std::make_unique<h264nal::H264BitstreamParser::BitstreamState>();
//            for (const auto &nalu_index : nalu_indices) {
//                auto nal_unit = h264nal::H264NalUnitParser::ParseNalUnit(
//                        &data[nalu_index.payload_start_offset], nalu_index.payload_size,
//                        &bitstream_parser_state, parsing_options);
//                if (nal_unit == nullptr) {
//                    spdlog::error("error while parsing buffer into nal unit");
//                    continue;
//                }
//
//                nal_unit->offset = nalu_index.payload_start_offset;
//                nal_unit->length = nalu_index.payload_size;
//                spdlog::info(
//                        "nal_unit [ start-offset: {7} offset: {0} length: {1} parsed_length: {2} checksum: 0x{3} "
//                        "] nal_unit_header [ forbidden_zero_bit: {4} nal_ref_idc: {5} "
//                        "nal_unit_type: {6} ]",
//                        nal_unit->offset, nal_unit->length, nal_unit->parsed_length,
//                        nal_unit->checksum->GetPrintableChecksum(),
//                        nal_unit->nal_unit_header->forbidden_zero_bit,
//                        nal_unit->nal_unit_header->nal_ref_idc,
//                        nal_unit->nal_unit_header->nal_unit_type,
//                        nalu_index.start_offset);
//                current_head = nalu_index.start_offset;
//            }

            int frames_returned{0};
//                spdlog::info("push frame - colorFrame timestamp: {0} index: {1} current-head: {2} payload-size: {3}",
//                             ts, idx, nalu_index.start_offset, nalu_index.payload_size);
            if (!decoder->decode_frame((uint8_t*)cf->data(),
                                       cf->dataSize(),
                                       frameCounter,
                                       cuda_stream.value(),
                                       frames_returned)) {
                spdlog::error("error while decoding segment: {0}", ts);
                return;
            }

            for (int i=0; i< frames_returned; ++i)
            {
                int64_t frame_ts{0};
                bool encoding_complete{false};
                encoding_complete = decoder->extract_frame((uint8_t*)output_image.data, image_size_bytes, cuda_stream.value(), frame_ts);
                if (encoding_complete) {
                    cv::cuda::Stream cv_cuda_stream = cv::cuda::StreamAccessor::wrapStream(cuda_stream.value());
                    FrameInfo info;
                    info.frame_idx = idx;
                    output_image.download(info.image, cv_cuda_stream);
                    if (frame_queue.push(std::move(info)) != pcpd::dataflow::channel_op_status::success) {
                        spdlog::error("error while pushing frame {0} into queue", idx);
                    }
                    spdlog::info("decoded frame - nvdec timestamp: {0} - colorFrame index: {1}, successful frames: {2}", frame_ts, info.frame_idx, frameCounter);
                }
            }

            frameCounter += frames_returned;
        } else {
            spdlog::error("invalid frame: no color image {0}", frameCounter);
        }
    };

    // Pass in the configuration and start the pipeline
    pipe->start(config, cb);


    while (frameCounter < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // stop the pipeline
    pipe->stop();
    should_stop = true;
    receiver.wait();
    return 0;
}
catch(ob::Error &e) {
    spdlog::error("Error: function: {0} args: {1}, msg: {2}, type: {3}", e.getName(), e.getArgs(), e.getMessage(), e.getExceptionType());
    exit(EXIT_FAILURE);
}