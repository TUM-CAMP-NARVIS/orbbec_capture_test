#pragma once

#include "tcn/vpf/ffmpeg.h"

namespace tcn::vpf
{
	class OpenCodec
	{
	public:

		OpenCodec(AVCodecContext* openCodecContext);
		~OpenCodec();

		AVCodecContext* GetContext();

	private:

		AVCodecContext* context;
	};


}
