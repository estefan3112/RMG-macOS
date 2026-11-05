# Bundled shaders

These RetroArch slang shaders are bundled with RMG for the paraLLEl-RDP
shader presets. They come unchanged from
[libretro/slang-shaders](https://github.com/libretro/slang-shaders)
(commit `3685eda59ffd2ebe1b12cc4640581dc23552ce88`), keeping its folder layout so the presets work as is.

| Preset | Author | License |
|---|---|---|
| `crt/crt-easymode.slangp` | EasyMode | GPL (see `crt/shaders/crt-easymode.slang`) |
| `crt/crt-cgwg-fast.slangp` | cgwg, Themaister | GPL-2.0-or-later (see `crt/shaders/crt-cgwg-fast.slang`) |
| `bezel/koko-aio/koko-aio-ng.slangp` (and its presets) | kokoko3k | GPL-3.0 (see `bezel/koko-aio/LICENSE`) |

`bezel/koko-aio/koko-aio-ng.slangp` has one change compared to upstream, taken from
[estefan3112/koko-aio-slang](https://github.com/estefan3112/koko-aio-slang): the background
textures (`bg_under`, `bg_over`, `backdrop`) use `clamp_to_edge`, so the background isn't
mirrored and repeated in windows wider than the background picture.

More shaders: any `.slangp` preset from the full slang-shaders collection
can be chosen with *Custom* in the paraLLEl-RDP settings.
