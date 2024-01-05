#include <utility>

#include "tcn/vpf/FFmpegException.h"

namespace tcn::vpf
{
	FFmpegException::FFmpegException(std::string  error) : errorInfo{std::move(error)}
	{
	}

	FFmpegException::FFmpegException(std::string  error, int returnValue): errorInfo{std::move(error)}
	{
	}
}