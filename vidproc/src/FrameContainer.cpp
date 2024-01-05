#include "tcn/vpf/FrameContainer.h"
#include "tcn/vpf/FFmpegException.h"

namespace tcn::vpf
{
	FrameContainer::FrameContainer(AVFrame* frame, const Rational& timeBase)
	{
		AVFrame *tmp = av_frame_clone(frame);
		if (!tmp) { throw FFmpegException("Failed to clone frame"); };
		av_frame_unref(frame);
		this->frame = tmp;
		timeBase_ = timeBase;
	}

	FrameContainer::~FrameContainer()
	{
		av_frame_free(&frame);
	}

	AVFrame* FrameContainer::GetFrame()
	{
		return frame;
	}

	Rational FrameContainer::GetTimeBase()
	{
		return timeBase_;
	}
}
