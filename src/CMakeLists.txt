add_executable(backlight-notify main.c)

set_target_properties(backlight-notify PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS OFF
)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(LIBNOTIFY REQUIRED libnotify)
pkg_search_module(GDKPIXBUF REQUIRED gdk-pixbuf-2.0)
pkg_search_module(XCB REQUIRED xcb)
pkg_search_module(XCB_RANDR REQUIRED xcb-randr)
pkg_search_module(XCB_ATOM REQUIRED xcb-atom)


target_link_libraries(
    backlight-notify
    ${GLIB_LDFLAGS}
    ${LIBNOTIFY_LIBRARIES}
)

target_include_directories(
    backlight-notify
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
)

