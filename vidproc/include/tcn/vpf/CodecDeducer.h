#pragma once

#include "tcn/vpf/ffmpeg.h"

namespace tcn::vpf
{
	struct CodecDeducer
	{
		static const AVCodec* DeduceEncoder(AVCodecID codecId);
		static const AVCodec* DeduceEncoder(const char* codecName);

		static const AVCodec* DeduceDecoder(AVCodecID codecId);
		static const AVCodec* DeduceDecoder(const char* codecName);
	};

}
