#pragma once

#include "tcn/vpf/Rational.h"
#include "tcn/vpf/Types.h"

namespace tcn::vpf
{
	struct StreamData
	{
		MediaType type;

		Rational timeBase;
		Rational frameRate;
	};
}