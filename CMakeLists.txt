cmake_minimum_required(VERSION 3.26)
project(orbbec_capture_test)

set(CMAKE_CXX_STANDARD 17)

find_package(orbbec-sdk REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenCV REQUIRED)
find_package(ffmpeg REQUIRED)


add_executable(orbbec_capture_test main.cpp H26xDecoder.cpp H26xDecoder.h buffered_channel.h)
target_link_libraries(orbbec_capture_test PRIVATE
        spdlog::spdlog
        ffmpeg::ffmpeg
        opencv::opencv
        orbbec-sdk::orbbec-sdk
)
target_include_directories(orbbec_capture_test PRIVATE
    ${PROJECT_SOURCE_DIR}/vidproc/include
)