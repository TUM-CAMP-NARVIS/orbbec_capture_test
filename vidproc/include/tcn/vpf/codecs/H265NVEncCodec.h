#pragma once
#include "tcn/vpf/codecs/VideoCodec.h"

namespace tcn::vpf
{

	class H265NVEncCodec : public VideoCodec
	{

	public:

		H265NVEncCodec();

		void SetPreset(const char* preset);
	};


}