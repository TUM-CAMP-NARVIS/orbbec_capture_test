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

#include <opencv2/opencv.hpp>

#include "buffered_channel.h"
#include "H26xDecoder.h"

std::unique_ptr<tcn::vpf::H26xDecoder> decoder;
size_t image_size_bytes{0};
std::shared_ptr<ob::FrameSet> currentFrameSet;
std::mutex                    frameSetMutex;
std::mutex                    displayMutex;
uint64_t frameCounter{0};

struct FrameInfo{
    uint64_t frame_idx{0};
    uint64_t dec_frame_idx{};
    cv::Mat image;
};
tcn::buffered_channel<FrameInfo> frame_queue{2};
tcn::buffered_channel<std::shared_ptr<ob::FrameSet>> frame_set_queue{8};

std::atomic<bool> should_stop{false};

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


static void avlog_cb(void *, int level, const char * szFmt, va_list varg) {
    char buffer [1024];
    vsnprintf(buffer, sizeof(buffer), szFmt, varg);
    std::string message(buffer);
    message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
    switch (level) {
        case AV_LOG_TRACE:
            spdlog::trace("ffmpeg: {0}", message);
            break;
        case AV_LOG_DEBUG:
        case AV_LOG_VERBOSE:
            spdlog::debug("ffmpeg: {0}", message);
            break;
        case AV_LOG_INFO:
            spdlog::info("ffmpeg: {0}", message);
            break;
        case AV_LOG_WARNING:
            spdlog::warn("ffmpeg: {0}", message);
            break;
        default:
            spdlog::error("ffmpeg: {0}", message);
            break;
    }
}

int main(int argc, char **argv) try {
    spdlog::set_level(spdlog::level::level_enum::info);
    ob::Context::setLoggerSeverity(OB_LOG_SEVERITY_INFO);
    av_log_set_callback(avlog_cb);
    av_log_set_level(AV_LOG_INFO);

    // Create a Context
    ob::Context ctx;

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


    auto display_cb = [&](cv::Mat image) {
        std::scoped_lock<std::mutex> lk(displayMutex);
        //spdlog::info("got display frame idx; {0} decoder idx: {1}", info.frame_idx, info.dec_frame_idx);
        cv::imshow("color", image);
        cv::waitKey(2);
    };

    auto decoder_task = std::async([&]() {
        spdlog::info("start decoder thread");
        while (!should_stop) {
            std::shared_ptr<ob::FrameSet> fs;
            auto ret = frame_set_queue.pop_wait_for(fs, std::chrono::milliseconds(5));
            if (ret == tcn::channel_op_status::timeout) {
                continue;
            } else if (ret == tcn::channel_op_status::success) {
                auto cf = fs->colorFrame();
                if (!cf) {
                    spdlog::error("invalid fs received in decoder thread");
                    continue;
                }
                if (cf->format() == OB_FORMAT_H264) {
                    if (!decoder) {

                        decoder = std::make_unique<tcn::vpf::H26xDecoder>(display_cb);
                        if (!decoder->DecoderInit(cf->format(), OB_FORMAT_BGRA)) {
                            spdlog::error("error initializing decoder");
                        }
                        spdlog::info("created decoder: {0}x{1}", cf->width(), cf->height());
                    }

                    auto t_start = std::chrono::system_clock::now();

                    if (decoder->DecodeOnePacket(static_cast<int>(cf->dataSize()),
                                                  (uint8_t*)cf->data())) {
                        ++frameCounter;
                    } else {
                        spdlog::info("something went wrong with decoding..");
                    }

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
        if (frame_set_queue.push_wait_for(std::move(fs), std::chrono::milliseconds((1000 / colorProfile->fps()) - 5)) != tcn::channel_op_status::success) {
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
//    receiver_task.wait();

    report_stats("frame_durations", frame_durations);
    report_stats("decode_durations", encode_durations);

    return 0;
}
catch(ob::Error &e) {
    spdlog::error("Error: function: {0} args: {1}, msg: {2}, type: {3}", e.getName(), e.getArgs(), e.getMessage(), e.getExceptionType());
    exit(EXIT_FAILURE);
}