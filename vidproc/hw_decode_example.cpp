#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
//#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <cstring>

extern "C" {
  //Linker errors if not inside extern. FFMPEG headers are not C++ aware
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libavutil/pixdesc.h>
  #include <libavutil/hwcontext.h>
  #include <libavutil/opt.h>
  #include <libavutil/avassert.h>
  #include <libavutil/imgutils.h>
}

#include <iomanip>
#include <string>
#include <sstream>
#include <opencv2/opencv.hpp>

#ifdef __cplusplus
extern "C" {
  #endif // __cplusplus
  #include <libavdevice/avdevice.h>
  #include <libavfilter/avfilter.h>
  #include <libavformat/avio.h>
  #include <libavutil/avutil.h>
  #include <libpostproc/postprocess.h>
  #include <libswresample/swresample.h>
  #include <libswscale/swscale.h>
  #ifdef __cplusplus
} // end extern "C".
#endif // __cplusplus

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file_fd = NULL;
cv::Mat output_mat;
int bgr_size;

static int hw_decoder_init(AVCodecContext *ctx,
  const enum AVHWDeviceType type) {
  int err = 0;

  if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
      NULL, NULL, 0)) < 0) {
    fprintf(stderr, "Failed to create specified HW device.\n");
    return err;
  }
  ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

  return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
  const enum AVPixelFormat *pix_fmts) {
  const enum AVPixelFormat *p;

  for (p = pix_fmts; *p != -1; p++) {
    if ( *p == hw_pix_fmt)
      return *p;
  }

  fprintf(stderr, "Failed to get HW surface format.\n");
  return AV_PIX_FMT_NONE;
}

static int decode_write(AVCodecContext *avctx, AVPacket *packet) {
  AVFrame *frame = NULL, *sw_frame = NULL;
  AVFrame *tmp_frame = NULL;
  uint8_t *buffer = NULL;
  //int size;
  int ret = 0;

  ret = avcodec_send_packet(avctx, packet);
  if (ret < 0) {
    fprintf(stderr, "Error during decoding\n");
    return ret;
  }

  while (1) {
    if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
      fprintf(stderr, "Can not alloc frame\n");
      ret = AVERROR(ENOMEM);
      av_frame_free(&frame);
      av_frame_free(&sw_frame);
      av_freep(&buffer);
      if (ret < 0) {
        return ret;
      }

    }

    ret = avcodec_receive_frame(avctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      av_frame_free(&frame);
      av_frame_free(&sw_frame);
      return 0;
    } else if (ret < 0) {
      fprintf(stderr, "Error while decoding\n");
      av_frame_free(&frame);
      av_frame_free(&sw_frame);
      av_freep(&buffer);
      if (ret < 0) {
        return ret;
      }

    }

    if (frame->format == hw_pix_fmt) {
      /* retrieve data from GPU to CPU */
      if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
        fprintf(stderr, "Error transferring the data to system memory\n");
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_freep(&buffer);
        if (ret < 0) {
          return ret;
        }

      }
      tmp_frame = sw_frame;
    } else {
      tmp_frame = frame;
    }

    AVPixelFormat format_to_use = AV_PIX_FMT_YUVJ420P;

    //Create SwsContext and allocate AVFrame in the first call to decode_write
    //Notes: 
    //1. It is just a "quick fix" that uses as an example - we should do it before decode_write.
    //2. We are also not executing sws_freeContext and av_frame_free (not a good practice).
    //////////////////////////////////////////////////////////////////////////////
    static bool is_first_time = true;
    static SwsContext* sws_context = NULL;
    static AVFrame *pBGRFrame = NULL;
    
    if (is_first_time)
    {
      is_first_time = false;
    
      //Create SwsContext for converting NV12 to BGR
      sws_context = sws_getContext(sw_frame->width,
          sw_frame->height,
          AV_PIX_FMT_NV12,
          sw_frame->width,
          sw_frame->height,
          AV_PIX_FMT_BGR24,   //OpenCV uses bgr24 pixel format
          SWS_FAST_BILINEAR,
          NULL,
          NULL,
          NULL);
    
      if (sws_context == NULL)
      {
          printf("Error: sws_context == NULL\n");
          return -1;  //Error!
      }
    
      //Allocate destination BGR frame
      pBGRFrame = av_frame_alloc();

      pBGRFrame->format = AV_PIX_FMT_BGR24;
      pBGRFrame->width = sw_frame->width;
      pBGRFrame->height = sw_frame->height;
      int sts = av_frame_get_buffer(pBGRFrame, 0);

      if (sts < 0)
      {
        printf("Error: av_frame_get_buffer sts = %d\n", sts);
        return -1;  //Error!
      }
    }
    //////////////////////////////////////////////////////////////////////////////

    //We can't use cvtColor because input Y and UV color planes (of NV12) are not consecutive in memory
    //////////////////////////////////////////////////////////////////////////////
    //cv::Mat mat_src = cv::Mat(sw_frame->height + (sw_frame->height / 2), sw_frame->width, CV_8UC1, sw_frame->data[0]);
    //cv::Mat out_mat;
    //cv::cvtColor(mat_src, out_mat, cv::COLOR_YUV2RGB_NV21);

    int response = sws_scale(sws_context,           //struct SwsContext *c, 
                             sw_frame->data,        //const uint8_t *const srcSlice[],
                             sw_frame->linesize,    //const int srcStride[], 
                             0,                     //int srcSliceY, 
                             sw_frame->height,      //int srcSliceH,
                             pBGRFrame->data,       //uint8_t *const dst[], 
                             pBGRFrame->linesize); //const int dstStride[]);

    if (response < 0)
    {
      printf("Error: sws_scale response = %d\n", response);
      return -1;
    }

    //Warp BGR frame with cv::Mat.
    cv::Mat out_mat = cv::Mat(sw_frame->height, sw_frame->width, CV_8UC3, pBGRFrame->data[0], pBGRFrame->linesize[0]);
    //////////////////////////////////////////////////////////////////////////////


    output_mat = out_mat;

    if (output_mat.empty() == false) {
      cv::imshow("image", output_mat);
      cv::waitKey(1);
    }

    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_freep(&buffer);
    return ret;
  }
}

//TEST_CASE("CUDAH264", "Tests hardware h264 decoding") {
int main()
{
  AVFormatContext *input_ctx = NULL;
  int video_stream, ret;
  AVStream *video = NULL;
  AVCodecContext *decoder_ctx = NULL;
  AVCodec *decoder = NULL;
  AVPacket *packet = NULL;
  enum AVHWDeviceType type;
  int i;

  std::string device_type = "cuda";
  //std::string input_file = "rtsp://10.100.2.152"; //My H264 network stream here...
  std::string input_file = "rtsp://127.0.0.1:40000/out";  //Use localhost IP for testing.

  /* The stream data is below...
  Input #0, rtsp, from 'rtsp://10.100.2.152':
    Metadata:
      title           : VCP IPC Realtime stream
    Duration: N/A, start: 0.000000, bitrate: N/A
    Stream #0:0: Video: h264 (High), yuvj420p(pc, bt709, progressive), 1920x1080, 10 fps, 10 tbr, 90k tbn, 20 tbc
  */

  type = av_hwdevice_find_type_by_name(device_type.c_str());
  if (type == AV_HWDEVICE_TYPE_NONE) {
    fprintf(stderr, "Device type %s is not supported.\n", device_type.c_str());
    fprintf(stderr, "Available device types:");
    while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
      fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
    fprintf(stderr, "\n");
    throw std::runtime_error("Error");
  }

  packet = av_packet_alloc();
  if (!packet) {
    fprintf(stderr, "Failed to allocate AVPacket\n");
    throw std::runtime_error("Error");
  }

  /* open the input file */
  if (avformat_open_input(&input_ctx, input_file.c_str(), NULL, NULL) != 0) {
    fprintf(stderr, "Cannot open input file '%s'\n", input_file.c_str());
    throw std::runtime_error("Error");
  }

  if (avformat_find_stream_info(input_ctx, NULL) < 0) {
    fprintf(stderr, "Cannot find input stream information.\n");
    throw std::runtime_error("Error");
  }

  av_dump_format(input_ctx, 0, input_file.c_str(), 0);

  for (int i = 0; i < (int)input_ctx->nb_streams; i++) {
    auto pCodec = avcodec_find_decoder(input_ctx->streams[i]->codecpar->codec_id);
    auto pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, input_ctx->streams[i]->codecpar);

    printf("Found Video stream with ID: %d\n", input_ctx->streams[i]->id);
    printf("\t Stream Index: %d\n", input_ctx->streams[i]->index);

    AVCodecParameters *codecpar = input_ctx->streams[i]->codecpar;
    printf("\t Codec Type: %s\n", av_get_media_type_string(codecpar->codec_type));
    printf("\t Side data count: %d\n", input_ctx->streams[i]->nb_side_data);
    printf("\t Pixel format: %i\n", input_ctx->streams[i]->codecpar->format);
    printf("\t Pixel Format Name: %s\n", av_get_pix_fmt_name((AVPixelFormat) input_ctx->streams[i]->codecpar->format));
    printf("\t Metadata count: %d\n", av_dict_count(input_ctx->streams[i]->metadata));
  }

  /* find the video stream information */
  ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
  if (ret < 0) {
    fprintf(stderr, "Cannot find a video stream in the input file\n");
    throw std::runtime_error("Error");
  }

  video_stream = ret;

  for (i = 0;; i++) {
    const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
    if (!config) {
      fprintf(stderr, "Decoder %s does not support device type %s.\n",
        decoder->name, av_hwdevice_get_type_name(type));
      throw std::runtime_error("Error");
    }
    if (config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
      config->device_type == type) {
      hw_pix_fmt = config->pix_fmt;
      break;
    }
  }

  if (!(decoder_ctx = avcodec_alloc_context3(decoder))) {
    throw std::runtime_error("NO MEMORY");
  }

  video = input_ctx->streams[video_stream];
  if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0) {
    throw std::runtime_error("Error");
  }

  decoder_ctx->get_format = get_hw_format;

  if (hw_decoder_init(decoder_ctx, type) < 0) {
    throw std::runtime_error("Error");
  }

  if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) {
    fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
    throw std::runtime_error("Error");
  }

  /* actual decoding and dump the raw data */
  while (ret >= 0) {
    if ((ret = av_read_frame(input_ctx, packet)) < 0)
      break;

    if (video_stream == packet->stream_index)
      ret = decode_write(decoder_ctx, packet);

    av_packet_unref(packet);
  }

  /* flush the decoder */
  ret = decode_write(decoder_ctx, NULL);

  if (output_file_fd) {
    fclose(output_file_fd);
  }
  av_packet_free(&packet);
  avcodec_free_context(&decoder_ctx);
  avformat_close_input(&input_ctx);
  av_buffer_unref(&hw_device_ctx);

  return 0;
}