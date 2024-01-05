#include "tcn/vpf/sinks/FrameSinkStream.h"
#include "tcn/vpf/FFmpegException.h"

namespace tcn::vpf
{

	FrameSinkStream::FrameSinkStream(FrameWriter* _frameSink, int _streamIndex)
	{
		frameSink = _frameSink;
		streamIndex = _streamIndex;
	}

	void FrameSinkStream::WriteFrame(AVFrame* frame, StreamData* metaData)
	{
		frameSink->WriteFrame(streamIndex, frame, metaData);
	}

	void FrameSinkStream::Close()
	{
		frameSink->Close(streamIndex);
	}

	bool FrameSinkStream::IsPrimed()
	{
		return frameSink->IsPrimed();
	}
}
