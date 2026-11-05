# librashader C header

`librashader.h` is the C API header of [librashader](https://github.com/SnowflakePowered/librashader)
v0.12.0 (`include/librashader.h`), used to run RetroArch slang shader presets on the
paraLLEl-RDP output. librashader is licensed under MPL-2.0 or GPL-3.0-only.

The library itself (`librashader.dylib` / `librashader.so`) is loaded at runtime,
when it's missing shaders are simply unavailable.
