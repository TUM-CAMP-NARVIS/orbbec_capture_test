#include "tcn/vpf/codecs/H265NVEncCodec.h"

namespace tcn::vpf
{

	H265NVEncCodec::H265NVEncCodec()
		: VideoCodec("hevc_nvenc")
	{

	}

	void H265NVEncCodec::SetPreset(const char* preset)
	{
		SetOption("preset", preset);
	}
}