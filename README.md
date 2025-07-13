# tshotkeytrigger

`tshotkeytrigger` is a CLI tool that triggers hotkey actions in [Teamspeak](https://teamspeak.com/) 6 via TeamSpeak Remote Applications API. 

Its primary purpose is to work around current limitations of TeamSpeak on Linux under Wayland compositors, the lack of support for global hotkeys. 
In these environments, hotkeys only work when the TeamSpeak window is focused.

This tool enables you to trigger actions such as "Toggle Mute" by calling TeamSpeak’s Remote API externally. You can then use your desktop environment's native global shortcut system to bind keys to run this tool.
For example on Gnome: [Gnome Keyboard Shortcuts](https://help.gnome.org/users/gnome-help/stable/keyboard-shortcuts-set.html.en)

> **Note:** This tool cannot simulate push-to-talk key presses, as it does not support holding a key down, only discrete press and release actions can be triggered.


## Installation

This tool is built to be run on Linux.

1. Install `libwebsockets` library.

Debian/Ubuntu
```
apt install libwebsockets19t64 
```

Arch Linux
```
pacman -S libwebsockets
```

2. Download the archive from the releases page, extract it and copy the executable to your selected directory.

## Usage

For a list of available options and usage instructions run:
```
tshotkeytrigger --help
```

### Initial set up

In TeamSpeak, ensure that Remote Apps are enabled - go to **Settings** -> **Remote Apps** -> **Enabled**.

To allow the tool to trigger an action, you’ll need to bind a virtual key press (sent by the tool) to a specific action in TeamSpeak. This can be configured under **Settings** -> **Key Bindings**. 

The application will guide you through creating a new trigger during setup.

Run the following command to set up a new trigger
```
tshotkeytrigger --button-id <button-id> --setup
```

`<button-id>` is a custom identifier of the virtual key you want to use. It can be any string you choose. For example "toggle.mute".


### Trigger

```
tshotkeytrigger --button-id <button-id>
```

## Development

### Requirements

* [Meson](https://mesonbuild.com) with Ninja backend
* C compiler - gcc
* [libwebsockets](https://libwebsockets.org/) development files installed

### Build

Set up the build directory in release mode
```
meson setup builddir --buildtype=release
```

Compile the project
```
meson compile -C builddir
```

## License
This project is licensed under the MIT License. See [LICENSE](./LICENSE) for full text.


## Acknowledgments

This tool uses:
* [libwebsockets](https://libwebsockets.org/) (MIT License) for WebSocket integration

See [THIRD_PARTY_LICENSES](./THIRD_PARTY_LICENSES).
