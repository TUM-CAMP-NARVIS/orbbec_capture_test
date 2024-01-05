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
#include <numeric>

#include <spdlog/spdlog.h>

#include "h264nal/h264_bitstream_parser.h"
#include "h264nal/h264_common.h"

#include "pcpd_codec_nvenc/stream_decoder.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda/common.hpp>
#include <opencv2/core/cuda_stream_accessor.hpp>

#include "pcpd_cuda_core/core.cuh"
#include "pcpd_core/dataflow/buffered_channel.h"

#include "H26xDecoder.h"

std::unique_ptr<pcpd::codec_nvenc::StreamDecoder> decoder;
cv::cuda::GpuMat output_image;
size_t image_size_bytes{0};
std::shared_ptr<ob::FrameSet> currentFrameSet;
std::mutex                    frameSetMutex;
uint64_t frameCounter{0};
std::optional<cudaStream_t> cuda_stream;

struct FrameInfo{
    uint64_t frame_idx{0};
    uint64_t dec_frame_idx{};
    cv::Mat image;
};
pcpd::dataflow::buffered_channel<FrameInfo> frame_queue{2};
pcpd::dataflow::buffered_channel<std::shared_ptr<ob::FrameSet>> frame_set_queue{8};

std::atomic<bool> should_stop{false};
bool first_keyframe_received{false};

std::vector<double> frame_durations;
std::vector<double> encode_durations;
std::chrono::time_point<std::chrono::system_clock> last_frame_ts;
bool is_first_frame{true};

void report_stats(const std::string& name, const std::vector<double> v) {
    if (v.empty()) {
        spdlog::info("{0} has no measurements", name);
        return;
    }
    double sum = std::accumulate(std::begin(v), std::end(v), 0.0);
    double m =  sum / v.size();

    double accum = 0.0;
    std::for_each (std::begin(v), std::end(v), [&](const double d) {
        accum += (d - m) * (d - m);
    });

    double stdev = sqrt(accum / (v.size()-1));
    auto max_duration = std::max_element(std::begin(v), std::end(v));
    auto min_duration = std::min_element(std::begin(v), std::end(v));

    spdlog::info("{0} stats - count: {1} mean: {2}, std-dev: {3}, min: {4}, max: {5}",
                 name, v.size(), m, stdev, *min_duration, *max_duration);

}

int main(int argc, char **argv) try {

    checkCudaErrors(cudaSetDevice(0));

    // Create a Context
    ob::Context ctx;
    ctx.setLoggerSeverity(OB_LOG_SEVERITY_INFO);

    // Enter the device ip address (currently only FemtoMega devices support network connection, and its default ip address is 192.168.1.10)
    std::string ip;
    std::cout << "Input your device ip(default: 10.0.60.40):";
    std::getline(std::cin, ip);
    if(ip.empty()) {
        ip = "10.0.60.40";
    }
    std::string use_depth_in;
    std::cout << "should depth stream be activated(default: y):";
    std::getline(std::cin, use_depth_in);
    if(use_depth_in.empty()) {
        use_depth_in = "y";
    }
    bool use_depth = use_depth_in == "y" || use_depth_in == "Y";


    auto receiver_task = std::async([&]() {
        spdlog::info("start receiver thread");
        while (!should_stop) {
            FrameInfo info{};
            auto ret = frame_queue.pop_wait_for(info, std::chrono::milliseconds(5));
            if (ret == pcpd::dataflow::channel_op_status::timeout) {
                continue;
            } else if (ret == pcpd::dataflow::channel_op_status::success) {
                //spdlog::info("got display frame idx; {0} decoder idx: {1}", info.frame_idx, info.dec_frame_idx);
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

    auto decoder_task = std::async([&]() {
        spdlog::info("start decoder thread");
        while (!should_stop) {
            std::shared_ptr<ob::FrameSet> fs;
            auto ret = frame_set_queue.pop_wait_for(fs, std::chrono::milliseconds(5));
            if (ret == pcpd::dataflow::channel_op_status::timeout) {
                continue;
            } else if (ret == pcpd::dataflow::channel_op_status::success) {
                auto cf = fs->colorFrame();
                if (!cf) {
                    spdlog::error("invalid fs received in decoder thread");
                    continue;
                }
                if (cf->format() == OB_FORMAT_H264) {

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
                        image_size_bytes = cf->width(), cf->height() * 4; // RGBA
                        spdlog::info("created output buffer: {0}x{1} ({2})", cf->width(), cf->height(), image_size_bytes);
                    }

                    auto t_start = std::chrono::system_clock::now();

                    //spdlog::info("got compressed frame: ts: {0} idx: {1}", cf->timeStamp(), cf->index());
                    int frames_returned{0};
                    if (!decoder->decode_frame((uint8_t*)cf->data(),
                                               cf->dataSize(),
                                               frameCounter,
                                               cuda_stream.value(),
                                               frames_returned)) {
                        spdlog::error("error while decoding segment: {0}", cf->timeStamp());
                        return;
                    }
                    //spdlog::info("decoding frame idx: {0} resulted in {1} images", cf->index(), frames_returned);

                    for (int i=0; i< frames_returned; ++i)
                    {
                        int64_t frame_ts{0};
                        bool encoding_complete{false};
                        encoding_complete = decoder->extract_frame((uint8_t*)output_image.data, image_size_bytes, cuda_stream.value(), frame_ts);
                        if (encoding_complete) {
                            cv::cuda::Stream cv_cuda_stream = cv::cuda::StreamAccessor::wrapStream(cuda_stream.value());
                            FrameInfo info;
                            info.frame_idx = cf->index();
                            info.dec_frame_idx = frameCounter + i;
                            output_image.download(info.image, cv_cuda_stream);
                            cudaStreamSynchronize(cuda_stream.value());
                            if (frame_queue.push(std::move(info)) != pcpd::dataflow::channel_op_status::success) {
                                spdlog::error("error while pushing frame {0} into queue", cf->index());
                            } else {
                                //spdlog::info("decoded frame ts: {0} nvdec timestamp: {1} - colorFrame index: {2}, nvdec index: {3}", cf->timeStamp(), frame_ts, cf->index(), frameCounter + i);
                            }
                        }
                    }

                    frameCounter += frames_returned;

                    auto t_diff = std::chrono::system_clock::now() - t_start;
                    auto t_diff_us = std::chrono::duration_cast<std::chrono::microseconds>(t_diff).count();
                    encode_durations.push_back(double(t_diff_us) / 1000.);

                } else {
                    spdlog::error("invalid frame: no color image {0}", frameCounter);
                }
            } else {
                spdlog::warn("unexpect buffer_channel return status.");
            }
        }
        spdlog::info("finish decoder thread");
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
    auto colorProfile = colorProfileList->getVideoStreamProfile(1280, 720, OB_FORMAT_H264, 25);
    // enable depth stream
    config->enableStream(colorProfile);

    if (use_depth) {
        // Get the depth camera configuration list
        auto depthProfileList = pipe->getStreamProfileList(OB_SENSOR_DEPTH);
        // use default configuration
        auto depthProfile = depthProfileList->getVideoStreamProfile(640, 576, OB_FORMAT_Y16, 25);
        // enable depth stream
        config->enableStream(depthProfile);
    }


    auto cb = [&](std::shared_ptr<ob::FrameSet> fs) {
        if (!fs) {
            spdlog::error("received invalid frameset");
            return;
        }
        // never forward incomplete frames
        if ((use_depth && fs->depthFrame() == nullptr) || fs->colorFrame() == nullptr) {
            spdlog::warn("received incomplete frame - skipping");
            return;
        }

        bool should_skip_frame{false};
        if (use_depth && !first_keyframe_received  && fs->colorFrame()->format() == OBFormat::OB_FORMAT_H264) {
            spdlog::debug("waiting for keyframe for color image");

            uint8_t* data = static_cast<uint8_t*>(fs->colorFrame()->data());
            int data_size = static_cast<int>(fs->colorFrame()->dataSize()); // maybe only scan the head of the packet?
            auto nalu_indices = h264nal::H264BitstreamParser::FindNaluIndices(data, data_size);

            h264nal::H264BitstreamParserState bitstream_parser_state;
            h264nal::ParsingOptions parsing_options;
            parsing_options.add_offset = false;
            parsing_options.add_length = false;
            parsing_options.add_parsed_length = false;
            parsing_options.add_checksum = true; /* options->add_checksum */
            auto bitstream = std::make_unique<h264nal::H264BitstreamParser::BitstreamState>();
            for (const auto &nalu_index: nalu_indices) {
                auto nal_unit = h264nal::H264NalUnitParser::ParseNalUnit(
                        &data[nalu_index.payload_start_offset], nalu_index.payload_size,
                        &bitstream_parser_state, parsing_options);
                if (nal_unit == nullptr) {
                    spdlog::error("error while parsing buffer into nal unit");
                    continue;
                }
                nal_unit->offset = nalu_index.payload_start_offset;
                nal_unit->length = nalu_index.payload_size;
                if (nal_unit->nal_unit_header->nal_unit_type == h264nal::NalUnitType::CODED_SLICE_OF_IDR_PICTURE_NUT) {
                    first_keyframe_received = true;
                    spdlog::info("found initial keyframe");
                }
            }
            should_skip_frame = !first_keyframe_received;
        }
        if (should_skip_frame) {
            spdlog::info("waiting for first keyframe of color image");
            return;
        }

        auto t_now = std::chrono::system_clock::now();
        auto t_diff = t_now - last_frame_ts;
        last_frame_ts = t_now;
        auto t_diff_us = std::chrono::duration_cast<std::chrono::microseconds>(t_diff).count();
        if (!is_first_frame) {
            // skip first frame as it includes the startup time..
            frame_durations.push_back(double(t_diff_us) / 1000.);
        } else {
            is_first_frame = false;
        }

        auto idx = fs->colorFrame()->index();
        if (frame_set_queue.push_wait_for(std::move(fs), std::chrono::milliseconds((1000 / colorProfile->fps()) - 5)) != pcpd::dataflow::channel_op_status::success) {
            spdlog::error("error while pushing frame {0} into queue", idx);
        }

    };

    // Pass in the configuration and start the pipeline
    if (use_depth) {
        pipe->enableFrameSync();
    }
    last_frame_ts = std::chrono::system_clock::now();
    pipe->start(config, cb);


    while (frame_durations.size() < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    // stop the pipeline
    pipe->stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    should_stop = true;
    decoder_task.wait();
    receiver_task.wait();

    report_stats("frame_durations", frame_durations);
    report_stats("encode_durations", encode_durations);

    return 0;
}
catch(ob::Error &e) {
    spdlog::error("Error: function: {0} args: {1}, msg: {2}, type: {3}", e.getName(), e.getArgs(), e.getMessage(), e.getExceptionType());
    exit(EXIT_FAILURE);
}