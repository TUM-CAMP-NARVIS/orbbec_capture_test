#include "tcn/vpf/codecs/H264NVEncCodec.h"

namespace tcn::vpf
{

	H264NVEncCodec::H264NVEncCodec()
		: VideoCodec("h264_nvenc")
	{

	}

	void H264NVEncCodec::SetPreset(const char* preset)
	{
		SetOption("preset", preset);
	}
}