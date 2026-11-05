#!/usr/bin/env bash
#
# Creates RMG.app (and optionally a .dmg) from a portable install.
#
# usage: Package/macOS/Create.sh [bin dir] [output dir]
#   bin dir    - portable install, default: Bin/Release
#   output dir - where RMG.app is created, default: Build/macOS
#
# environment variables:
#   CODESIGN_IDENTITY - signing identity, default: "-" (ad-hoc signature)
#                       a real identity also enables the hardened runtime
#   CREATE_DMG        - set to 1 to also create a .dmg next to RMG.app
#   MACDEPLOYQT       - path to macdeployqt, default: macdeployqt from PATH
#   RMG_VERSION       - version string, default: git describe or VERSION file
#
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
toplvl_dir="$(cd "$script_dir/../.." && pwd)"
bin_dir="$(cd "${1:-$toplvl_dir/Bin/Release}" && pwd)"
out_dir="${2:-$toplvl_dir/Build/macOS}"
identity="${CODESIGN_IDENTITY:--}"
macdeployqt="${MACDEPLOYQT:-macdeployqt}"

version="${RMG_VERSION:-$(git -C "$toplvl_dir" describe --tags --always 2>/dev/null || cat "$toplvl_dir/VERSION")}"
version="${version#v}"
short_version="${version%%-*}"
min_macos="${MACOSX_DEPLOYMENT_TARGET:-$(sw_vers -productVersion | cut -d. -f1).0}"

app="$out_dir/RMG.app"
contents="$app/Contents"

is_mach_o()
{
    file -b "$1" | grep -q "Mach-O"
}

echo "==> Creating $app (version $version)"
rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Resources" "$contents/Frameworks"

# 1. copy the portable install, RMG-Core finds the
#    shared data in Contents/Resources/Data when it
#    runs from an app bundle
rsync -a --exclude "/Data" "$bin_dir/" "$contents/MacOS/"
cp -R "$bin_dir/Data" "$contents/Resources/Data"

# 2. deploy Qt and all other (Homebrew) libraries into the bundle,
#    this has to happen before step 3, macdeployqt doesn't accept
#    @executable_path references in anything but the executable.
#
#    macdeployqt would deploy *every* Qt plugin it can find (QML,
#    virtual keyboard, pdf, ...), so only the plugins RMG needs
#    are copied, and macdeployqt runs with -no-plugins
qt_plugins=(
    platforms/libqcocoa.dylib
    styles/libqmacstyle.dylib
    imageformats/libqsvg.dylib
    iconengines/libqsvgicon.dylib
)
qt_plugin_dirs=()
qtpaths="$(dirname "$macdeployqt")/qtpaths"
if [ -x "$qtpaths" ]
then
    qt_plugin_dirs+=("$("$qtpaths" --query QT_INSTALL_PLUGINS)")
fi
if command -v brew > /dev/null
then
    qt_plugin_dirs+=("$(brew --prefix)/share/qt/plugins")
fi

for plugin in "${qt_plugins[@]}"
do
    found=""
    for dir in "${qt_plugin_dirs[@]}"
    do
        if [ -f "$dir/$plugin" ]
        then
            found="$dir/$plugin"
            break
        fi
    done
    if [ -z "$found" ]
    then
        echo "error: Qt plugin $plugin not found in: ${qt_plugin_dirs[*]}"
        exit 1
    fi
    mkdir -p "$contents/PlugIns/$(dirname "$plugin")"
    cp -L "$found" "$contents/PlugIns/$plugin"
    chmod u+w "$contents/PlugIns/$plugin"
done
printf '[Paths]\nPlugins = PlugIns\n' > "$contents/Resources/qt.conf"

deploy_args=()
while IFS= read -r -d '' f
do
    [ "$f" = "$contents/MacOS/RMG" ] && continue
    is_mach_o "$f" || continue
    deploy_args+=("-executable=$f")
done < <(find "$contents/MacOS" "$contents/PlugIns" -type f -print0)
echo "==> Running macdeployqt"
"$macdeployqt" "$app" -no-plugins "${deploy_args[@]}" -verbose=1

# 3. ensure there's exactly one libRMG-Core, in Frameworks, and point
#    RMG and every plugin at it, the plugins have to share the
#    RMG-Core instance that RMG itself uses
core_ref="@executable_path/../Frameworks/libRMG-Core.dylib"
if [ -f "$contents/MacOS/libRMG-Core.dylib" ]
then
    if [ -f "$contents/Frameworks/libRMG-Core.dylib" ]
    then # macdeployqt already copied it
        rm -f "$contents/MacOS/libRMG-Core.dylib"
    else
        mv "$contents/MacOS/libRMG-Core.dylib" "$contents/Frameworks/"
    fi
fi
install_name_tool -id "$core_ref" "$contents/Frameworks/libRMG-Core.dylib"
while IFS= read -r -d '' f
do
    is_mach_o "$f" || continue
    [ "$f" = "$contents/Frameworks/libRMG-Core.dylib" ] && continue
    while IFS= read -r old_ref
    do
        if [ -n "$old_ref" ] && [ "$old_ref" != "$core_ref" ]
        then
            install_name_tool -change "$old_ref" "$core_ref" "$f"
        fi
    done < <(otool -L "$f" | tail -n +2 | awk '/libRMG-Core\.dylib/ { print $1 }')
done < <(find "$contents/MacOS" "$contents/Frameworks" -type f -not -path "*.framework/*" -print0)

# 4. macdeployqt leaves @rpath references of some Homebrew libraries
#    alone (i.e libbrotlidec -> @rpath/libbrotlicommon with an rpath of
#    @loader_path/../lib), point those directly at the copy in Frameworks
while IFS= read -r -d '' f
do
    is_mach_o "$f" || continue
    while IFS= read -r ref
    do
        lib="${ref#@rpath/}"
        if [ -f "$contents/Frameworks/$lib" ]
        then
            install_name_tool -change "$ref" "@loader_path/$lib" "$f"
        fi
    done < <(otool -L "$f" | tail -n +2 | awk '$1 ~ /^@rpath\// && $1 !~ /\.framework\// { print $1 }')
done < <(find "$contents/Frameworks" -maxdepth 1 -type f -name "*.dylib" -print0)
# and give RMG an rpath to Frameworks as well, for anything
# that still uses @rpath (i.e the Qt frameworks)
install_name_tool -add_rpath "@executable_path/../Frameworks" "$contents/MacOS/RMG" 2> /dev/null || true

# 5. MoltenVK (Vulkan on Metal) for paraLLEl-RDP, RMG loads it from
#    Contents/Frameworks at runtime, so nothing links against it and
#    macdeployqt doesn't deploy it by itself
moltenvk="${MOLTENVK_LIB:-}"
if [ -z "$moltenvk" ] && command -v brew > /dev/null
then
    moltenvk="$(brew --prefix molten-vk 2> /dev/null)/lib/libMoltenVK.dylib"
fi
if [ -n "$moltenvk" ] && [ -f "$moltenvk" ]
then
    cp -L "$moltenvk" "$contents/Frameworks/libMoltenVK.dylib"
    chmod u+w "$contents/Frameworks/libMoltenVK.dylib"
    install_name_tool -id "@executable_path/../Frameworks/libMoltenVK.dylib" "$contents/Frameworks/libMoltenVK.dylib"
else
    echo "warning: MoltenVK not found (brew install molten-vk), paraLLEl-RDP won't work"
fi

# 6. librashader (optional) runs slang shader presets on the paraLLEl-RDP
#    output, the plugin loads it from Contents/Frameworks at runtime
librashader="${LIBRASHADER_LIB:-}"
if [ -z "$librashader" ]
then
    for candidate in /opt/homebrew/lib/librashader.dylib /usr/local/lib/librashader.dylib
    do
        [ -f "$candidate" ] && librashader="$candidate" && break
    done
fi
if [ -z "$librashader" ] && [ "${LIBRASHADER_DOWNLOAD:-1}" = "1" ]
then
    # fetch the prebuilt release once and keep it in a cache folder
    librashader_version="${LIBRASHADER_VERSION:-0.12.0}"
    case "$(uname -m)" in
        arm64) librashader_arch="aarch64" ;;
        *)     librashader_arch="x86_64" ;;
    esac
    librashader_cache="$HOME/Library/Caches/RMG-build/librashader-$librashader_version-$librashader_arch"
    if [ ! -f "$librashader_cache/librashader.dylib" ]
    then
        librashader_zip="librashader-$librashader_arch-macos-v$librashader_version-optimized.zip"
        echo "==> Downloading librashader $librashader_version"
        mkdir -p "$librashader_cache"
        if curl -fsSL -o "$librashader_cache/$librashader_zip" \
            "https://github.com/SnowflakePowered/librashader/releases/download/librashader-v$librashader_version/$librashader_zip" &&
           unzip -q -o "$librashader_cache/$librashader_zip" -d "$librashader_cache/zip"
        then
            found="$(find "$librashader_cache/zip" -name librashader.dylib | head -n 1)"
            [ -n "$found" ] && cp "$found" "$librashader_cache/librashader.dylib"
        fi
        rm -rf "$librashader_cache/zip" "$librashader_cache/$librashader_zip"
    fi
    [ -f "$librashader_cache/librashader.dylib" ] && librashader="$librashader_cache/librashader.dylib"
fi
if [ -n "$librashader" ] && [ -f "$librashader" ]
then
    cp -L "$librashader" "$contents/Frameworks/librashader.dylib"
    chmod u+w "$contents/Frameworks/librashader.dylib"
    install_name_tool -id "@executable_path/../Frameworks/librashader.dylib" "$contents/Frameworks/librashader.dylib"
else
    echo "note: librashader not found or downloaded (set LIBRASHADER_LIB), shader presets won't be available"
fi

# fail when anything still links against Homebrew, or references
# a library that doesn't exist in the bundle, such a bundle only
# works on the build machine (or not at all)
leftovers=""
while IFS= read -r -d '' f
do
    is_mach_o "$f" || continue
    # otool -L also lists a library's own install name, skip that one
    own_id="$(otool -D "$f" | tail -n +2)"
    refs="$(otool -L "$f" | tail -n +2 | awk -v id="$own_id" '$1 != id' | grep -E "^[[:space:]]*(/opt/homebrew|/usr/local)/" || true)"
    if [ -n "$refs" ]
    then
        leftovers+="$f:"$'\n'"$refs"$'\n'
    fi
    # @loader_path and @executable_path references must exist
    dir="$(dirname "$f")"
    while IFS= read -r ref
    do
        case "$ref" in
            @loader_path/*)     target="$dir/${ref#@loader_path/}" ;;
            @executable_path/*) target="$contents/MacOS/${ref#@executable_path/}" ;;
            *) continue ;;
        esac
        if [ ! -e "$target" ]
        then
            leftovers+="$f:"$'\n'"	$ref (missing)"$'\n'
        fi
    done < <(otool -L "$f" | tail -n +2 | awk -v id="$own_id" '$1 != id { print $1 }')
done < <(find "$app" -type f -print0)
if [ -n "$leftovers" ]
then
    echo "error: these binaries still link against libraries outside the bundle:"
    echo "$leftovers"
    exit 1
fi

# 7. Info.plist and icon
sed -e "s/@RMG_VERSION@/$short_version/g" \
    -e "s/@RMG_MIN_MACOS@/$min_macos/g" \
    "$script_dir/Info.plist.in" > "$contents/Info.plist"
if command -v rsvg-convert > /dev/null
then
    iconset="$(mktemp -d)/RMG.iconset"
    mkdir -p "$iconset"
    for size in 16 32 128 256 512
    do
        rsvg-convert -w "$size" -h "$size" "$toplvl_dir/Package/com.github.Rosalie241.RMG.svg" \
            -o "$iconset/icon_${size}x${size}.png"
        rsvg-convert -w "$((size * 2))" -h "$((size * 2))" "$toplvl_dir/Package/com.github.Rosalie241.RMG.svg" \
            -o "$iconset/icon_${size}x${size}@2x.png"
    done
    iconutil -c icns "$iconset" -o "$contents/Resources/RMG.icns"
else
    echo "warning: rsvg-convert not found, RMG.app will have no icon"
fi

# 8. code signing, inside-out
echo "==> Signing with identity: $identity"
sign_args=(--force --sign "$identity")
if [ "$identity" != "-" ]
then
    sign_args+=(--options runtime --timestamp)
fi

# loose binaries first (plugins, core, dylibs, Qt plugins)
while IFS= read -r -d '' f
do
    case "$f" in
        *.framework/*) continue ;;
    esac
    [ "$f" = "$contents/MacOS/RMG" ] && continue
    is_mach_o "$f" || continue
    codesign "${sign_args[@]}" "$f"
done < <(find "$contents" -type f -print0)

# then the frameworks
while IFS= read -r -d '' framework
do
    codesign "${sign_args[@]}" "$framework"
done < <(find "$contents/Frameworks" -maxdepth 1 -name "*.framework" -print0)

# and finally the app itself, with the JIT entitlement
codesign "${sign_args[@]}" --entitlements "$script_dir/RMG.entitlements" "$app"
codesign --verify --deep --strict --verbose=2 "$app"

# 9. optional .dmg
if [ "${CREATE_DMG:-0}" = "1" ]
then
    dmg="$out_dir/RMG-macOS-$version.dmg"
    staging="$(mktemp -d)"
    cp -R "$app" "$staging/"
    ln -s /Applications "$staging/Applications"
    rm -f "$dmg"
    hdiutil create -volname "RMG" -srcfolder "$staging" -ov -format UDZO "$dmg"
    if [ "$identity" != "-" ]
    then
        codesign --force --sign "$identity" --timestamp "$dmg"
    fi
    echo "==> Created $dmg"
fi

echo "==> Done: $app"
