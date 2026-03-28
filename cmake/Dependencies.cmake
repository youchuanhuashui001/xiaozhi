include(FindPkgConfig)

if (NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "pkg-config is required to resolve native dependencies")
endif()

pkg_check_modules(LIBWEBSOCKETS REQUIRED IMPORTED_TARGET libwebsockets)
pkg_check_modules(OPUS REQUIRED IMPORTED_TARGET opus)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)

set(XIAOZHI_THIRD_PARTY_TARGETS
    PkgConfig::LIBWEBSOCKETS
    PkgConfig::OPUS
    PkgConfig::ALSA
)
