from conan import ConanFile


class TestOrbbecCaptureRecipe(ConanFile):
    name = "orbbec_capture_test"
    version = "0.1"

    python_requires = "camp_common/[>=0.5 <1.0]@camposs/stable"
    python_requires_extend = "camp_common.CampCMakeBase"

    settings = "os", "compiler", "build_type", "arch"

    default_options = {
        "ffmpeg/*:with_ssl": False,
    }

    def configure(self):
        if self.settings.os == "Macos":
            self.options["opencv"].with_ipp = False
            self.options["ffmpeg"].with_videotoolbox = True

    def requirements(self):
        self.requires("orbbec-sdk/1.8@vendor/stable", transitive_libs=True)
        self.requires("spdlog/1.11.0", transitive_libs=True)
        self.requires("ffmpeg/6.1@camposs/stable", transitive_libs=True)
        self.requires("opencv/4.9.0@camposs/stable", transitive_libs=True)
        self.requires("fmt/9.1.0", force=True)
        self.requires("libwebp/1.3.2", force=True)

