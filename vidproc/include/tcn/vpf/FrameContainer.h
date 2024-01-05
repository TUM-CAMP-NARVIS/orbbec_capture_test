#pragma once

#include "tcn/vpf/Rational.h"
#include "tcn/vpf/ffmpeg.h"

namespace tcn::vpf
{
	class FrameContainer
	{
	public:

		FrameContainer(AVFrame* frame, const Rational& timeBase);
		~FrameContainer();

		AVFrame* GetFrame();
		Rational GetTimeBase();

	private:

		AVFrame* frame;
		Rational timeBase_;
	};

}

