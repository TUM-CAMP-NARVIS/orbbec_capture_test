#pragma once

#include "tcn/vpf/Rational.h"
#include "tcn/vpf/codecs/Codec.h"
#include "tcn/vpf/OpenCodec.h"

namespace tcn::vpf
{

	class VideoCodec : public Codec
	{
	public:

		VideoCodec(const char* codecName);
		VideoCodec(AVCodecID codecId);
		virtual ~VideoCodec();

		OpenCodec* Open(int width, int height, const Rational& frameRate, AVPixelFormat format);

		// This maps to the qscale parameter so should be in the range [0,31].
		void SetQualityScale(int qscale);

		bool IsPixelFormatSupported(AVPixelFormat format);
		bool IsFrameRateSupported(const Rational& frameRate);

		AVPixelFormat GetDefaultPixelFormat();
		Rational GetClosestSupportedFrameRate(const Rational& frameRate);

	};


}