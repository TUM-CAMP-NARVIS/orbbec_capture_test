#pragma once
#include "tcn/vpf/codecs/VideoCodec.h"

namespace tcn::vpf
{

	class H264NVEncCodec : public VideoCodec
	{

	public:

		H264NVEncCodec();

		void SetPreset(const char* preset);
	};


}