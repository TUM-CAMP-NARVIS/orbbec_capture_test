#pragma once

#include "tcn/vpf/ffmpeg.h"
#include "tcn/vpf/sinks/FrameSinkStream.h"

namespace tcn::vpf
{
	class FrameSink
	{
	public:

		virtual FrameSinkStream* CreateStream() = 0;

		virtual MediaType GetMediaType() = 0;

		virtual ~FrameSink() {}
	};


}
