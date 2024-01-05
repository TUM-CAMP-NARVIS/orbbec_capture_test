cmake_minimum_required(VERSION 3.27)
project(orbbec_capture_test)

set(CMAKE_CXX_STANDARD 17)

find_package(orbbec-sdk REQUIRED)
find_package(h264nal REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenCV REQUIRED)
find_package(ffmpeg REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nvidia-video-codec-sdk REQUIRED)
find_package(pcpd_codec_nvenc REQUIRED)


set(TCN_VPF_SOURCES
        ${PROJECT_SOURCE_DIR}/vidproc/src/codecs/Codec.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/codecs/H264NVEncCodec.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/codecs/H265NVEncCodec.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/codecs/VideoCodec.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/demuxing/StreamData.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/sinks/FrameSinkStream.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/sources/RawVideoDataSource.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/CodecDeducer.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/FFmpegException.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/FrameContainer.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/OpenCodec.cpp
        ${PROJECT_SOURCE_DIR}/vidproc/src/Rational.cpp

)

set(TCN_VPF_HEADERS
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/codecs/Codec.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/codecs/H264NVEncCodec.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/codecs/H265NVEncCodec.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/codecs/VideoCodec.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/demuxing/StreamData.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/sinks/FrameSink.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/sinks/FrameSinkStream.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/sinks/FrameWriter.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/sinks/VideoFrameSink.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/sources/RawVideoDataSource.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/ffmpeg.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/CodecDeducer.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/FFmpegException.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/FrameContainer.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/OpenCodec.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/Rational.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/StandardLibraryIncludes.h
        ${PROJECT_SOURCE_DIR}/vidproc/include/tcn/vpf/Types.h
)


add_executable(orbbec_capture_test main.cpp ${TCN_VPF_SOURCES} ${TCN_VPF_HEADERS})
target_link_libraries(orbbec_capture_test PRIVATE
        ffmpeg::ffmpeg
        orbbec-sdk::orbbec-sdk
        h264nal::h264nal
        spdlog::spdlog
        pcpd_codec_nvenc::pcpd_codec_nvenc
        opencv::opencv
        nvidia-video-codec-sdk::nvidia-video-codec-sdk
)
target_include_directories(orbbec_capture_test PRIVATE
    ${PROJECT_SOURCE_DIR}/vidproc/include
)