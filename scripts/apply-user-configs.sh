#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIGS_DIR="$SCRIPT_DIR/configs"

echo "Applying user configurations from $CONFIGS_DIR..."

mkdir -p "$HOME/.config/caelestia" "$HOME/.config/foot"

# Caelestia configs
if [ -d "$CONFIGS_DIR/caelestia" ]; then
    cp -v "$CONFIGS_DIR"/caelestia/*.json "$HOME/.config/caelestia/"
fi

# KDE configs
if [ -f "$CONFIGS_DIR/kde/powerdevilrc" ]; then
    cp -v "$CONFIGS_DIR/kde/powerdevilrc" "$HOME/.config/powerdevilrc"
fi
if [ -f "$CONFIGS_DIR/kde/kscreenlockerrc" ]; then
    cp -v "$CONFIGS_DIR/kde/kscreenlockerrc" "$HOME/.config/kscreenlockerrc"
fi

# Foot config
if [ -f "$CONFIGS_DIR/foot/foot.ini" ]; then
    cp -v "$CONFIGS_DIR/foot/foot.ini" "$HOME/.config/foot/foot.ini"
fi

echo "User configurations applied successfully."
