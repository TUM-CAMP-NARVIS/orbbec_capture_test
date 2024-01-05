#include "tcn/vpf/CodecDeducer.h"
#include "tcn/vpf/FFmpegException.h"
#include <string>

namespace tcn::vpf
{
    const AVCodec* CodecDeducer::DeduceEncoder(const char* codecName)
	{
        const AVCodec* codec = avcodec_find_encoder_by_name(codecName);
		if (!codec)
		{
			throw FFmpegException("Codec " + std::string(codecName) + " not found");
		}
		return codec;
	}

    const AVCodec* CodecDeducer::DeduceEncoder(AVCodecID codecId)
	{
        const AVCodec* codec = avcodec_find_encoder(codecId);
		if (!codec)
		{
			throw FFmpegException("Codec with id " + std::to_string((int)codecId) + " not found");
		}
		return codec;
	}

    const AVCodec* CodecDeducer::DeduceDecoder(const char* codecName)
	{
        const AVCodec* codec = avcodec_find_decoder_by_name(codecName);
		if (!codec)
		{
			throw FFmpegException("Codec " + std::string(codecName) + " not found");
		}
		return codec;
	}

	const AVCodec* CodecDeducer::DeduceDecoder(AVCodecID codecId)
	{
		if (codecId == AV_CODEC_ID_NONE) return nullptr;
        const AVCodec* codec = avcodec_find_decoder(codecId);
		if (!codec)
		{
			throw FFmpegException("Codec with id " + std::to_string((int)codecId) + " not found");
		}
		return codec;
	}
}
