#pragma once

namespace tcn::vpf {
    // This is the representation of the internal AVMediaType
    enum class MediaType{
        AUDIO,
        VIDEO,
        SUBTITLE,
        DATA,
        NB,
        UNKNOWN
    };
} // ffmpegcpp