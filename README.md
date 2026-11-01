# RMG for macOS (Apple Silicon)

> **This is an unofficial fork of [Rosalie's Mupen GUI](https://github.com/Rosalie241/RMG) with a native macOS port for Apple Silicon Macs.**
> The macOS work lives in the [`macos`](https://github.com/estefan3112/RMG-macOS/tree/macos) branch; `master` mirrors upstream.
> Please report macOS issues here, not to the upstream RMG project.

### What works

- Native arm64 build as a regular `RMG.app` (Qt 6, SDL3), started from Finder or Terminal
- **mupen64plus dynamic recompiler on Apple Silicon**, including a fix for the arm64 dynarec that is also submitted upstream as [mupen64plus/mupen64plus-core#1198](https://github.com/mupen64plus/mupen64plus-core/pull/1198)
- **GLideN64** (OpenGL 4.1) with resizing, fullscreen, VSync, FXAA, anisotropic filtering and hires textures
- **paraLLEl-RSP** with its JIT (MAP_JIT), both as RSP plugin and as automatic fallback for the HLE RSP plugin (e.g. Conker's Bad Fur Day)
- RSP HLE and CXD4, RMG's audio and input plugins, GameCube adapter input
- macOS Game Mode in fullscreen

Tested on a Mac mini M2 Pro with macOS 26 with Super Mario 64, Conker's Bad Fur Day, Banjo-Tooie, Star Wars: Rogue Squadron, Gauntlet Legends, Yoshi's Story and Wipeout 64.

### Not (yet) available on macOS

- paraLLEl-RDP (Vulkan, needs MoltenVK) and softrdp
- MSAA in GLideN64 (use FXAA instead); threaded video and async shader compilation
- raphnetraw input, netplay, voice recognition (VRU)

### Building

Requirements: Xcode Command Line Tools and [Homebrew](https://brew.sh).

```bash
brew install cmake ninja pkgconf qt sdl3 libusb libsamplerate speexdsp minizip freetype libpng librsvg
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"

cmake -S . -B Build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DPORTABLE_INSTALL=ON -DUSE_CCACHE=OFF
cmake --build Build/Release --parallel "$(sysctl -n hw.ncpu)"
cmake --install Build/Release --strip --prefix "$PWD"
MACDEPLOYQT="$(brew --prefix qt)/bin/macdeployqt" bash Package/macOS/Create.sh Bin/Release Build/macOS
```

The app bundle is created in `Build/macOS/RMG.app`. Settings and saves are stored in `~/Library/Application Support/RMG`.

### Tips

- **Window size on Retina/HiDPI displays:** GLideN64's windowed resolution is in pixels, so enter double the window size you want (e.g. `2560 x 1920` for a 1280 x 960 window) and save it to the *User* profile.
- **CPU emulator:** choose *Dynamic Recompiler* in Settings → Core for full speed.

### Credits

All credit for RMG goes to [Rosalie](https://github.com/Rosalie241) and the RMG contributors, and to the mupen64plus, GLideN64 and paraLLEl teams. The macOS port was written with AI assistance (Claude Opus 5.5, Anthropic) and built and tested by [estefan3112](https://github.com/estefan3112). More about N64 and arcade emulation on the Mac: [mameonmacs.blogspot.com](https://mameonmacs.blogspot.com)

---

# Rosalie's Mupen GUI

Rosalie's Mupen GUI is a free and open-source mupen64plus front-end written in C++.

It offers a simple-to-use user interface.

## Download

#### Windows
You can download Rosalie's Mupen GUI on [Github Releases](https://github.com/Rosalie241/RMG/releases)

#### Linux
You can download Rosalie's Mupen GUI on 
* [Github Releases (AppImage)](https://github.com/Rosalie241/RMG/releases)
* [Flathub (Flatpak)](https://flathub.org/apps/details/com.github.Rosalie241.RMG)
* [Arch User Repository](https://aur.archlinux.org/packages/rmg)
* [Fedora COPR](https://copr.fedorainfracloud.org/coprs/rosalie/RMG/)

## Netplay

You can use netplay with the following methods:

### Port Forwarding

1) Download [rmg-netplay-server](https://github.com/Rosalie241/rmg-netplay-server/releases/latest)
2) Port forward the ports stated in the [rmg-netplay-server README](https://github.com/Rosalie241/rmg-netplay-server)
3) Create a JSON file with something similar to the following contents, but using your public IP instead of a local one:
```
{
  "some-server-name": "127.0.0.1"
}
```
If you want to add multiple servers instead:
```
{
  "some-server-name": "127.0.0.1",
  "some-server-name-2": "127.0.0.1"
}
```
4) Store the JSON file or upload it somewhere
5) Enter the JSON file path or URL in the "Server list URL" text box in the RMG settings dialog
6) The specified servers in the JSON file should appear in the RMG GUI


### Tunneling Software

1) Download [rmg-netplay-server](https://github.com/Rosalie241/rmg-netplay-server/releases/latest)
2) Download [Radmin VPN](https://www.radmin-vpn.com/)
3) Run and create or join a network in Radmin VPN
4) The host should run [rmg-netplay-server](https://github.com/Rosalie241/rmg-netplay-server)
5) The server should show up as `local-server` within the RMG GUI

## Support

You can ask for help or report issues on [Github](https://github.com/Rosalie241/RMG/issues/new)

## License

Rosalie's Mupen GUI is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.en.html).

## Showcase

![RomBrowser](Package/Screenshots/RomBrowser.png)
![InGame](Package/Screenshots/InGame.png)
![InputSettings](Package/Screenshots/InputSettings.png)

## Building

#### Linux
* Portable Debian/Ubuntu

  ```bash
  sudo apt-get -y install cmake libusb-1.0-0-dev libhidapi-dev libsamplerate0-dev libspeex-dev libminizip-dev libsdl3-dev libfreetype6-dev libgl1-mesa-dev libglu1-mesa-dev pkg-config zlib1g-dev binutils-dev libspeexdsp-dev qt6-base-dev qt6-websockets-dev libqt6svg6-dev libvulkan-dev build-essential nasm git zip ninja-build
  ./Source/Script/Build.sh Release
  ```
  
* Portable Fedora
  ```bash
  sudo dnf install libusb1-devel hidapi-devel libsamplerate-devel minizip-compat-devel SDL3-devel freetype-devel mesa-libGL-devel mesa-libGLU-devel pkgconfig zlib-ng-devel binutils-devel speexdsp-devel qt6-qtbase-devel qt6-qtsvg-devel qt6-qtwebsockets-devel vulkan-devel gcc-c++ nasm git ninja-build
  ./Source/Script/Build.sh Release
  ```

* Portable Arch Linux
  ```bash
  sudo pacman -S --needed make cmake gcc libusb hidapi freetype2 libpng qt6 sdl3 libsamplerate nasm minizip pkgconf vulkan-headers git
  ./Source/Script/Build.sh Release
  ```

* Portable OpenSUSE Tumbleweed
  ```bash
  sudo zypper install SDL3-devel cmake freetype2-devel gcc gcc-c++ libusb-1_0-devel libhidapi-devel libhidapi-hidraw0 libpng16-devel libsamplerate-devel make nasm ninja pkgconf-pkg-config speex-devel vulkan-devel zlib-devel qt6-tools-devel qt6-opengl-devel qt6-widgets-devel qt6-svg-devel minizip-devel git
  ./Source/Script/Build.sh Release
  ```

When it's done building, executables can be found in `Bin/Release`

* Installation/Packaging
```bash
export src_dir="$(pwd)"
export build_dir="$(pwd)/build"
mkdir -p "$build_dir"
cmake -S "$src_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE="Release" -DPORTABLE_INSTALL="OFF" -DCMAKE_INSTALL_PREFIX="/usr" -G "Ninja"
cmake --build "$build_dir"
cmake --install "$build_dir" --prefix="/usr"
```

#### Windows
* Download & Install [MSYS2](https://www.msys2.org/) (UCRT64)
```bash
pacman -S --needed make mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libusb mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-freetype mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-qt6 mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-speexdsp mingw-w64-ucrt-x86_64-libsamplerate mingw-w64-ucrt-x86_64-libusb mingw-w64-ucrt-x86_64-nasm mingw-w64-ucrt-x86_64-minizip mingw-w64-ucrt-x86_64-vulkan-headers git
./Source/Script/Build.sh Release
```

When it's done building, executables can be found in `Bin/Release`
