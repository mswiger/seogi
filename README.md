# Seogi

Hangul IME for Wayland, based on [wlhangul](https://github.com/emersion/wlhangul/) by Simon Ser.

## Building

Seogi depends on libhangul, libsystemd, libxkbcommon, and Wayland.

    meson build
    ninja -C build
    build/seogi

## Usage

The following environment variables need to be set in order for the IME to work:

    export GTK_IM_MODULE=wayland
    export QT_IM_MODULE=wayland
    export XMODIFIERS="@im=wayland"

Additionally, your Wayland compositor must support the
[input-method-unstable-v2](https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/protocol/input-method-unstable-v2.xml)
and
[virtual-keyboard-unstable-v1](https://gitlab.freedesktop.org/wlroots/wlroots/-/blob/master/protocol/virtual-keyboard-unstable-v1.xml)
protocols.
Currently only Sway has been tested.

After running `seogi`, the mode can be toggled with the Hangul key by default.
The toggle key can be modified as follows:

    seogi -k Alt_R

The list of keys that can be used for the toggle key can be found in
[xkbcommon-keysyms.h](https://code.woboq.org/qt5/include/xkbcommon/xkbcommon-keysyms.h.html)
from libxkbcommon.
Remove the `XKB_KEY_` prefix from the key name and pass it to `seogi` through the `-k` parameter.

For a more complete list of parameters, run `seogi --help`.

Seogi includes a D-Bus service for observing its state (i.e., whether or not Hangul mode is enabled).
See [seogi-waybar](https://github.com/mswiger/seogi-waybar) for an example of this in action.

## License

MIT
