#include "tcn/vpf/codecs/Codec.h"
#include "tcn/vpf/FFmpegException.h"
#include "tcn/vpf/CodecDeducer.h"

namespace tcn::vpf
{
	Codec::Codec(const char* codecName)
	{
		const AVCodec* codec = CodecDeducer::DeduceEncoder(codecName);
		codecContext = LoadContext(codec);
	}

	Codec::Codec(AVCodecID codecId)
	{
		const AVCodec* codec = CodecDeducer::DeduceEncoder(codecId);
		codecContext = LoadContext(codec);
	}

	void Codec::SetOption(const char* name, const char* value)
	{
		av_opt_set(codecContext->priv_data, name, value, 0);
	}

	void Codec::SetOption(const char* name, int value)
	{
		av_opt_set_int(codecContext->priv_data, name, value, 0);
	}

	void Codec::SetOption(const char* name, double value)
	{
		av_opt_set_double(codecContext->priv_data, name, value, 0);
	}

	AVCodecContext* Codec::LoadContext(const AVCodec* codec)
	{
		AVCodecContext* _codecContext = avcodec_alloc_context3(codec);
		if (!_codecContext)
		{
			CleanUp();
			throw FFmpegException("Could not allocate video codec context for codec " + std::string(codec->name));
		}

		// copy the type
        _codecContext->codec_type = codec->type;

		return _codecContext;
	}

	void Codec::CleanUp()
	{
		if (codecContext != nullptr && !opened)
		{
			avcodec_free_context(&codecContext);
		}
	}

	OpenCodec* Codec::Open()
	{
		if (opened)
		{
			throw FFmpegException("You can only open a codec once");
		}

		int ret = avcodec_open2(codecContext, codecContext->codec, nullptr);
		if (ret < 0)
		{
			throw FFmpegException("Could not open codecContext for codec", ret);
		}

		opened = true;

		return new OpenCodec(codecContext);
	}

	Codec::~Codec()
	{
		CleanUp();
	}

	void Codec::SetGlobalContainerHeader()
	{
		if (opened) throw FFmpegException("This flag must be set before opening the codec");
		codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
}
