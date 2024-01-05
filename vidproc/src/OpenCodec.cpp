#include "tcn/vpf/OpenCodec.h"
#include "tcn/vpf/FFmpegException.h"

namespace tcn::vpf
{
	OpenCodec::OpenCodec(AVCodecContext* context)
	{
		if (!avcodec_is_open(context))
		{
			throw FFmpegException("Codec context for " + std::string(context->codec->name) + " hasn't been opened yet");
		}

		this->context = context;
	}

	OpenCodec::~OpenCodec()
	{
		avcodec_free_context(&context);
	}

	AVCodecContext* OpenCodec::GetContext()
	{
		return context;
	}
}
