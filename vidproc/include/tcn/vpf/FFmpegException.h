#pragma once

#include "tcn/vpf/ffmpeg.h"

#include "tcn/vpf/StandardLibraryIncludes.h"

namespace tcn::vpf
{
	class FFmpegException : public std::exception
	{

	public:

		explicit FFmpegException(std::string  error);

		FFmpegException(std::string  error, int returnValue);

		[[nodiscard]] virtual char const* what() const noexcept
		{
			return errorInfo.c_str();
		}


	private:

		std::string errorInfo;
	};
}