#!/bin/bash
# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
# Build the universal Release VST3 and point FL Studio at that one bundle.
# Quit FL Studio and Plugin Manager before running this. An open manager
# writes the old scan record back when it quits.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CMAKE="${CMAKE:-/opt/homebrew/bin/cmake}"
if [[ ! -x "$CMAKE" ]]; then
  CMAKE="$(command -v cmake)"
fi

BUILD="$ROOT/build/fl-release"
NAME="RONIN"
BUNDLE="$BUILD/Ronin_artefacts/Release/VST3/$NAME.vst3"
BINARY="$BUNDLE/Contents/MacOS/$NAME"
USER_DIR="${HOME}/Library/Audio/Plug-Ins/VST3"
USER_LINK="$USER_DIR/$NAME.vst3"
SYSTEM_COPY="/Library/Audio/Plug-Ins/VST3/$NAME.vst3"
DB="${HOME}/Documents/Image-Line/FL Studio/Presets/Plugin database/Installed"
SDK="$ROOT/build/_deps/juce-src/modules/juce_audio_processors/format_types/VST3_SDK"

# The Mac binary is pluginmanager (lowercase). pgrep -x is case-sensitive.
if pgrep -x pluginmanager >/dev/null 2>&1 || pgrep -x PluginManager >/dev/null 2>&1 \
  || pgrep -x OsxFL >/dev/null 2>&1 || pgrep -x "FL Studio" >/dev/null 2>&1; then
  echo "Quit FL Studio and Plugin Manager, then run scripts/install_fl_plugin.sh again." >&2
  exit 1
fi

if [[ -e "$SYSTEM_COPY" || -L "$SYSTEM_COPY" ]]; then
  echo "Remove ${SYSTEM_COPY} first. FL Studio lists that copy separately from the user folder." >&2
  exit 1
fi

if [[ -e "$USER_LINK" && ! -L "$USER_LINK" ]]; then
  echo "Refusing to replace a real bundle at ${USER_LINK}. This script only updates a symlink." >&2
  exit 1
fi

if [[ ! -d "$SDK" ]]; then
  echo "JUCE SDK headers are missing at ${SDK}. Configure the Debug build once so FetchContent can populate them." >&2
  exit 1
fi

cmake_args=(
  -S "$ROOT"
  -B "$BUILD"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
)
if [[ -d "$ROOT/build/_deps/juce-src" ]]; then
  cmake_args+=("-DFETCHCONTENT_SOURCE_DIR_JUCE=$ROOT/build/_deps/juce-src")
fi

echo "Building universal Release VST3"
"$CMAKE" "${cmake_args[@]}"
"$CMAKE" --build "$BUILD" --target Ronin_VST3 --parallel

if [[ ! -f "$BINARY" ]]; then
  echo "Missing ${BINARY}" >&2
  exit 1
fi

lipo_info="$(lipo -info "$BINARY")"
echo "$lipo_info"
case "$lipo_info" in
  *arm64*) ;;
  *) echo "Release bundle has no arm64 slice." >&2; exit 1 ;;
esac
case "$lipo_info" in
  *x86_64*) ;;
  *) echo "Release bundle has no x86_64 slice. FL Studio's bridge cannot load it." >&2; exit 1 ;;
esac

moduleinfo="$BUNDLE/Contents/Resources/moduleinfo.json"
if ! grep -q '"Fx"' "$moduleinfo"; then
  echo "moduleinfo.json has no Fx subcategory." >&2
  exit 1
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/ronin-fl-check.XXXXXX")"
cleanup() { rm -rf "$work"; }
trap cleanup EXIT

echo "Checking arm64 and x86_64 slices"
clang++ -std=c++20 -Wall -Wextra -Wno-deprecated-declarations -I "$SDK" -arch arm64 \
  "$ROOT/tools/vst3_load_check.cpp" -ldl -o "$work/check-arm64"
clang++ -std=c++20 -Wall -Wextra -Wno-deprecated-declarations -I "$SDK" -arch x86_64 \
  "$ROOT/tools/vst3_load_check.cpp" -ldl -o "$work/check-x64"
arch -arm64 "$work/check-arm64" "$BUNDLE"
arch -x86_64 "$work/check-x64" "$BUNDLE"

sine_out="$(/usr/bin/python3 "$ROOT/scripts/sine_through_fx.py" --vst3 "$BUNDLE" 2>&1)" || sine_status=$?
sine_status=${sine_status:-0}
printf '%s\n' "$sine_out"
if [[ "$sine_status" -ne 0 ]] || printf '%s\n' "$sine_out" | grep -q 'SINE_HOST FAIL'; then
  echo "Sine host check failed." >&2
  exit 1
fi
if printf '%s\n' "$sine_out" | grep -qx 'SINE_HOST PASS'; then
  :
elif printf '%s\n' "$sine_out" | grep -qx 'SINE_HOST SKIP'; then
  echo "Pedalboard sine check skipped. The slice check above still processed a dry block." >&2
else
  echo "Sine host check failed." >&2
  exit 1
fi

mkdir -p "$USER_DIR"
ln -sfn "$BUNDLE" "$USER_LINK"
if [[ "$(readlink "$USER_LINK")" != "$BUNDLE" ]]; then
  echo "Symlink ${USER_LINK} does not point at the Release bundle." >&2
  exit 1
fi
echo "Checking the user-folder symlink FL Studio opens"
arch -arm64 "$work/check-arm64" "$USER_LINK"
arch -x86_64 "$work/check-x64" "$USER_LINK"
echo "symlink ${USER_LINK} -> ${BUNDLE}"

remove_record() {
  local record="$1"
  if [[ -f "$record" ]]; then
    rm -f "$record"
    echo "removed ${record}"
  fi
}

# scanflags 1, a plugin type, a guid, and this symlink. A newer binary makes it stale.
effects_record_is_current() {
  local nfo="$DB/Effects/VST3/$NAME.nfo"
  [[ -f "$nfo" ]] || return 1
  grep -E -q '^ps_file_scanflags_0=1$' "$nfo" || return 1
  grep -E -q '^ps_file_type_0=' "$nfo" || return 1
  grep -E -q '^ps_file_guid_0=' "$nfo" || return 1
  grep -F -x -q "ps_file_filename_0=${USER_LINK}" "$nfo" || return 1
  if [[ "$BINARY" -nt "$nfo" ]]; then
    return 1
  fi
  return 0
}

for ext in nfo fst; do
  remove_record "$DB/Generators/VST3/$NAME.$ext"
  remove_record "$DB/Generators/New/$NAME.$ext"
done

if effects_record_is_current; then
  echo "Keeping the verified Effects record. This bundle is not newer than that scan."
  cat <<EOF
FL_INSTALL_OK
The verified Effects record is still in place.
Do not copy the bundle into /Library/Audio/Plug-Ins/VST3.
EOF
else
  for ext in nfo fst; do
    remove_record "$DB/Effects/VST3/$NAME.$ext"
    remove_record "$DB/Effects/New/$NAME.$ext"
  done
  cat <<EOF
FL_INSTALL_OK
In FL Studio: Options, Manage plugins, Find plugins.
The stored Effects record does not match this bundle, so it was removed.
Find plugins writes a new record from this universal bundle.
Do not copy the bundle into /Library/Audio/Plug-Ins/VST3.
EOF
fi
