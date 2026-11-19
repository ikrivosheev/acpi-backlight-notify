# acpi-backlight-notify

<a href="https://aur.archlinux.org/packages/acpi-backlight-notify"><img src="https://raw.githubusercontent.com/themix-project/oomox/master/packaging/download_aur.png" height="54"></a>

### Description

ACPI backlight notification daemon. Watches `/sys/class/backlight/<backlight>/actual_brightness` and shows a desktop notification (via libnotify) when the brightness changes.

### Dependencies

- GLib 2
- libnotify
- CMake 3.10+
- C compiler with C99 support

### Build

```
cmake -B build .
cmake --build build
```

To create a DEB package: `cpack` from the build directory (or `cpack --config build/CPackConfig.cmake`).

### Usage

```
backlight-notify [OPTION…]
```

### Options

#### Help Options:

- `-h`, `--help` - Show help options

#### Application Options:

- `-b`, `--backlight` - Folder name in `/sys/class/backlight/<backlight>` (**required**)
- `-d`, `--debug` - Enable debug information
- `-t`, `--timeout` - Notification timeout in seconds (-1 - default notification timeout, 0 - notification never expires)

### Example

```
backlight-notify -b intel_backlight
```

### License

[MIT](LICENSE)
