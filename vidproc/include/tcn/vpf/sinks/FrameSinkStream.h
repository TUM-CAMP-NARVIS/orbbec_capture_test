#pragma once

#include "tcn/vpf/ffmpeg.h"
#include "tcn/vpf/sinks/FrameWriter.h"
#include "tcn/vpf/demuxing/StreamData.h"

namespace tcn::vpf
{
	class FrameSinkStream
	{
	public:

		FrameSinkStream(FrameWriter* frameSink, int streamIdx);

		void WriteFrame(AVFrame* frame, StreamData* metaData);

		void Close();

		bool IsPrimed();

	private:

		FrameWriter* frameSink;
		int streamIndex;
	};
}
