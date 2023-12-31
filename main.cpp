#include "libobsensor/ObSensor.hpp"
#include "libobsensor/hpp/Error.hpp"

#include <thread>
#include <mutex>
#include <iostream>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

#include "h264nal/h264_bitstream_parser.h"
#include "h264nal/h264_common.h"

#include "pcpd_codec_nvenc/stream_decoder.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda/common.hpp>
#include <opencv2/core/cuda_stream_accessor.hpp>

#include "pcpd_cuda_core/core.cuh"

std::unique_ptr<pcpd::codec_nvenc::StreamDecoder> decoder;
cv::cuda::GpuMat output_image;
size_t image_size_bytes{0};
std::shared_ptr<ob::FrameSet> currentFrameSet;
std::mutex                    frameSetMutex;

int main(int argc, char **argv) try {

    cudaStream_t cuda_stream;
    checkCudaErrors(cudaSetDevice(0));
    cudaStreamCreate ( &cuda_stream );
    cv::cuda::Stream cv_cuda_stream = cv::cuda::StreamAccessor::wrapStream(cuda_stream);

    // Create a Context
    ob::Context ctx;

    // Enter the device ip address (currently only FemtoMega devices support network connection, and its default ip address is 192.168.1.10)
    std::string ip;
    std::cout << "Input your device ip(default: 10.0.60.40):";
    std::getline(std::cin, ip);
    if(ip.empty()) {
        ip = "10.0.60.40";
    }

    // Create a network device through ip (the default port number is: 8090, devices that currently support network mode do not support modifying the port
    // number)
    auto device = ctx.createNetDevice(ip.c_str(), 8090);

    // pass in device to create pipeline
    auto pipe = std::make_shared<ob::Pipeline>(device);

    // Create Config for configuring Pipeline work
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

//    // Get the depth camera configuration list
//    auto depthProfileList = pipe->getStreamProfileList(OB_SENSOR_DEPTH);
//    // use default configuration
//    auto depthProfile = depthProfileList->getProfile(OB_PROFILE_DEFAULT);
//    // enable depth stream
//    config->enableStream(depthProfile);

    // Get the color camera configuration list
    auto colorProfileList = pipe->getStreamProfileList(OB_SENSOR_COLOR);
    // use default configuration
    auto colorProfile = colorProfileList->getProfile(OB_PROFILE_DEFAULT);
    // enable depth stream
    config->enableStream(colorProfile);

    // Pass in the configuration and start the pipeline
    pipe->start(config);


    int idx{0};
    while (idx < 500) {
        auto fs = pipe->waitForFrames(40);
        if (!fs) {
            continue;
        }

        auto cf = fs->colorFrame();
        auto ts = cf->timeStamp();

        OBFormat fmt = colorProfile->format();
        if (cf && fmt == OB_FORMAT_H264) {
            if (!decoder) {
                decoder = std::make_unique<pcpd::codec_nvenc::StreamDecoder>("testdecoder");
                if (!decoder->initialize(pcpd::datatypes::BitstreamEncodingType::H264,
                                         pcpd::datatypes::PixelFormatType::RGBA,
                                         cf->width(), cf->height(), 32)) {
                    spdlog::error("error initializing decoder.");
                    exit(1);
                }
                output_image = cv::cuda::GpuMat(cv::Size(cf->width(), cf->height()), CV_8UC4);
                image_size_bytes = cf->width() * cf->height() * 4;
            }

            auto data = static_cast<uint8_t*>(cf->data());
            auto nalu_indices =
                    h264nal::H264BitstreamParser::FindNaluIndices(data, cf->dataSize());
            h264nal::H264BitstreamParserState bitstream_parser_state;
            h264nal::ParsingOptions parsing_options;
            auto bitstream =
                    std::make_unique<h264nal::H264BitstreamParser::BitstreamState>();
            for (const auto &nalu_index : nalu_indices) {
                auto nal_unit = h264nal::H264NalUnitParser::ParseNalUnit(
                        &data[nalu_index.payload_start_offset], nalu_index.payload_size,
                        &bitstream_parser_state, parsing_options);
                if (nal_unit == nullptr) {
                    spdlog::error("error while parsing buffer into nal unit");
                    continue;
                }
                spdlog::info(
                        "nal_unit [ offset: {0} length: {1} parsed_length: {2} checksum: 0x{3} "
                        "] nal_unit_header [ forbidden_zero_bit: {4} nal_ref_idc: {5} "
                        "nal_unit_type: {6} ]",
                        nal_unit->offset, nal_unit->length, nal_unit->parsed_length,
                        nal_unit->checksum->GetPrintableChecksum(),
                        nal_unit->nal_unit_header->forbidden_zero_bit,
                        nal_unit->nal_unit_header->nal_ref_idc,
                        nal_unit->nal_unit_header->nal_unit_type);
            }

            int frames_returned{0};
            if (!decoder->decode_frame((uint8_t*)cf->data(),
                                       cf->dataSize(),
                                       idx,
                                       cuda_stream,
                                       frames_returned)) {
                spdlog::error("error while decoding segment: {0}", ts);
                continue;
            }
            if(frames_returned >= 1){
                int64_t frame_ts{0};
                bool encoding_complete = decoder->extract_frame((uint8_t*)output_image.data, image_size_bytes, cuda_stream, frame_ts);
                spdlog::info("decoded frame with ts: {0} - {1}", frame_ts, ts);
                cv::Mat image, display_image;
                output_image.download(image, cv_cuda_stream);
                cv::cvtColor(image, display_image, cv::COLOR_RGBA2BGR);
                cv::imshow("color", display_image);
                cv::waitKey(5);
                ++idx;
            } else {
                spdlog::error("no frame decoded: {0}", ts);
            }
            spdlog::info("------------- loop ({0} - {1}) ----------------", idx, ts);
        } else {
            spdlog::error("invalid frame: no color image {0}", idx);
        }
    }

    // stop the pipeline
    pipe->stop();
    return 0;
}
catch(ob::Error &e) {
    spdlog::error("Error: function: {0} args: {1}, msg: {2}, type: {3}", e.getName(), e.getArgs(), e.getMessage(), e.getExceptionType());
    exit(EXIT_FAILURE);
}