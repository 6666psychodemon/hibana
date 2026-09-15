#!/bin/bash
# Installs Hibana for the current user. Run from Terminal:  bash install.sh
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"
mkdir -p "$VST3_DIR" "$AU_DIR" "$APP_DIR"

rm -rf "$VST3_DIR/Hibana.vst3" "$AU_DIR/Hibana.component" "$APP_DIR/Hibana.app"
cp -R "$DIR/Hibana.vst3"      "$VST3_DIR/"
cp -R "$DIR/Hibana.component" "$AU_DIR/"
cp -R "$DIR/Hibana.app"       "$APP_DIR/"

# Files downloaded from the internet are quarantined; unsigned-by-Apple plugins
# won't load until the flag is removed.
xattr -dr com.apple.quarantine "$VST3_DIR/Hibana.vst3" "$AU_DIR/Hibana.component" "$APP_DIR/Hibana.app" 2>/dev/null || true

# Make Logic / GarageBand notice the new AU right away.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed:"
echo "  VST3        -> $VST3_DIR/Hibana.vst3"
echo "  AU          -> $AU_DIR/Hibana.component"
echo "  Standalone  -> $APP_DIR/Hibana.app"
echo "Rescan plugins in your DAW if it was open."
