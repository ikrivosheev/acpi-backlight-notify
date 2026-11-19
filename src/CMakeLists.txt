add_executable(backlight-notify main.c)

set_target_properties(backlight-notify PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS OFF
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(LIBNOTIFY REQUIRED IMPORTED_TARGET libnotify)

target_link_libraries(backlight-notify PRIVATE PkgConfig::GLIB PkgConfig::LIBNOTIFY)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(backlight-notify PRIVATE -Wall -Wextra)
endif()
