cmake_minimum_required(VERSION 3.27)
project(orbbec_capture_test)

set(CMAKE_CXX_STANDARD 17)

find_package(orbbec-sdk REQUIRED)
find_package(h264nal REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenCV REQUIRED)
find_package(nvidia-video-codec-sdk REQUIRED)
find_package(pcpd_codec_nvenc REQUIRED)

add_executable(orbbec_capture_test main.cpp)
target_link_libraries(orbbec_capture_test
        orbbec-sdk::orbbec-sdk
        h264nal::h264nal
        spdlog::spdlog
        pcpd_codec_nvenc::pcpd_codec_nvenc
        opencv::opencv
        nvidia-video-codec-sdk::nvidia-video-codec-sdk)