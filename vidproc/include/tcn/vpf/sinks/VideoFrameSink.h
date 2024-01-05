#pragma once

#include "tcn/vpf/sinks/FrameSink.h"

namespace tcn::vpf
{
	class VideoFrameSink : public FrameSink
	{
	public:

		virtual MediaType GetMediaType()
		{
			return MediaType::VIDEO;
		}

		virtual ~VideoFrameSink() {}
	};
}
