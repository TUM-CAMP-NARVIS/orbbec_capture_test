from conan import ConanFile


class TestOrbbecCaptureRecipe(ConanFile):
    name = "orbbec_capture_test"
    version = "0.1"

    python_requires = "camp_common/[>=0.5 <1.0]@camposs/stable"
    python_requires_extend = "camp_common.CampCMakeBase"

    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("orbbec-sdk/1.8@vendor/stable", transitive_libs=True)
        self.requires("h264nal/0.15@camposs/stable", transitive_libs=True)
        self.requires("spdlog/1.11.0", transitive_libs=True)
        self.requires("fmt/9.1.0", force=True, transitive_libs=True)
        self.requires("ffmpeg/6.1@camposs/stable", transitive_libs=True)
        self.requires("pcpd_codec_nvenc/0.4.0@artekmed/stable", transitive_libs=True)
        self.requires("opencv/4.8.0@camposs/stable", transitive_libs=True)
        self.requires("nvidia-video-codec-sdk/12.1.14@vendor/stable", transitive_libs=True)
        self.requires("optick/1.4.0.0@camposs/stable", transitive_libs=True)
        self.requires("openssl/1.1.1t", transitive_libs=True)

