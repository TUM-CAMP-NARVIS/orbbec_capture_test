#pragma once

#include "tcn/vpf/ffmpeg.h"
#include "tcn/vpf/demuxing/StreamData.h"

namespace tcn::vpf
{
	class FrameWriter
	{
	public:

		virtual void WriteFrame(int streamIndex, AVFrame* frame, StreamData* metaData) = 0;

		virtual void Close(int streamIndex) = 0;

		virtual bool IsPrimed() = 0;
	};


}
