#!/usr/bin/env bash
#
# setup_env.sh — Bootstrap the full build/flash environment for SentinelOS.
#
# After cloning the repo, run this once to get everything needed to flash the
# board: ESP-IDF, the Xtensa toolchain, cmake/ninja, and the board submodule.
# The script is idempotent — safe to re-run; it skips steps already done.
#
# Usage:
#   tools/setup_env.sh              # set up the environment only
#   tools/setup_env.sh --build      # ...then build the firmware
#   tools/setup_env.sh --flash      # ...then build and flash the board
#
# Configurable via environment variables:
#   IDF_BRANCH   ESP-IDF branch/tag to install   (default: release/v6.0)
#   IDF_DIR      Where ESP-IDF is installed       (default: ~/esp/esp-idf-v6)
#   IDF_TARGET   Chip target                      (default: esp32)
#   PORT         Serial port for flashing         (default: /dev/ttyUSB0)
#
set -euo pipefail

# --- paths -------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
FIRMWARE="$ROOT/firmware"

IDF_BRANCH="${IDF_BRANCH:-release/v6.0}"
IDF_DIR="${IDF_DIR:-$HOME/esp/esp-idf-v6}"
IDF_TARGET="${IDF_TARGET:-esp32}"
PORT="${PORT:-/dev/ttyUSB0}"

# --- pretty logging ----------------------------------------------------------
if [ -t 1 ]; then
    C_BLUE=$'\033[1;34m'; C_GREEN=$'\033[1;32m'; C_YELLOW=$'\033[1;33m'
    C_RED=$'\033[1;31m'; C_RESET=$'\033[0m'
else
    C_BLUE=''; C_GREEN=''; C_YELLOW=''; C_RED=''; C_RESET=''
fi
step() { echo; echo "${C_BLUE}==>${C_RESET} $*"; }
ok()   { echo "${C_GREEN}  ✓${C_RESET} $*"; }
warn() { echo "${C_YELLOW}  ! ${C_RESET}$*"; }
die()  { echo "${C_RED}  ✗ $*${C_RESET}" >&2; exit 1; }

# --- args --------------------------------------------------------------------
DO_BUILD=0
DO_FLASH=0
for arg in "$@"; do
    case "$arg" in
        --build) DO_BUILD=1 ;;
        --flash) DO_BUILD=1; DO_FLASH=1 ;;
        -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "Unknown argument: $arg (use --build, --flash, or --help)" ;;
    esac
done

echo "${C_GREEN}============================================================${C_RESET}"
echo "${C_GREEN} SentinelOS — Environment Setup${C_RESET}"
echo "${C_GREEN}============================================================${C_RESET}"
echo "  ESP-IDF branch : $IDF_BRANCH"
echo "  ESP-IDF dir    : $IDF_DIR"
echo "  Target chip    : $IDF_TARGET"

# --- 1. prerequisites --------------------------------------------------------
step "Checking prerequisites"
for tool in git python3; do
    command -v "$tool" >/dev/null 2>&1 || die "'$tool' is required but not found on PATH."
    ok "$tool: $(command -v "$tool")"
done

# --- 2. ESP-IDF --------------------------------------------------------------
step "Installing ESP-IDF ($IDF_BRANCH)"
if [ -f "$IDF_DIR/export.sh" ]; then
    ok "ESP-IDF already present at $IDF_DIR"
else
    mkdir -p "$(dirname "$IDF_DIR")"
    echo "  Cloning ESP-IDF (this downloads a few GB, be patient)..."
    git clone -b "$IDF_BRANCH" --recursive https://github.com/espressif/esp-idf.git "$IDF_DIR"
    ok "ESP-IDF cloned to $IDF_DIR"
fi

# --- 3. toolchain ------------------------------------------------------------
step "Installing Xtensa toolchain for $IDF_TARGET"
"$IDF_DIR/install.sh" "$IDF_TARGET"
ok "Toolchain installed"

# Load the IDF environment for the rest of this script.
# shellcheck disable=SC1091
source "$IDF_DIR/export.sh" >/dev/null 2>&1 || die "Failed to source $IDF_DIR/export.sh"

# --- 4. cmake + ninja --------------------------------------------------------
step "Ensuring cmake and ninja are available"
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    warn "cmake/ninja not on PATH — installing them via ESP-IDF tools"
    python3 "$IDF_DIR/tools/idf_tools.py" install cmake ninja
    # shellcheck disable=SC1091
    source "$IDF_DIR/export.sh" >/dev/null 2>&1
fi
command -v cmake >/dev/null 2>&1 && ok "cmake: $(cmake --version | head -1)"  || die "cmake still missing"
command -v ninja >/dev/null 2>&1 && ok "ninja: $(ninja --version)"            || die "ninja still missing"

# --- 5. board submodule ------------------------------------------------------
step "Initializing board submodule (esp32-2432S028R)"
git -C "$ROOT" submodule update --init --recursive
if [ -f "$FIRMWARE/third_party/esp32-2432S028R/main/lcd.c" ]; then
    ok "Board submodule ready"
else
    warn "Board submodule looks empty — check .gitmodules / network access"
fi

# --- 6. Secure Boot signing key ---------------------------------------------
step "Checking Secure Boot V2 signing key"
KEY_PATH="$(grep -E '^CONFIG_SECURE_BOOT_SIGNING_KEY=' "$FIRMWARE/sdkconfig" 2>/dev/null \
            | head -1 | sed -E 's/^[^=]+="?([^"]*)"?/\1/')"
# ESP-IDF resolves a relative signing-key path against the firmware/ project dir.
case "${KEY_PATH:-}" in
    /*|"") : ;;                       # absolute or empty — leave as-is
    *) KEY_PATH="$FIRMWARE/$KEY_PATH" ;;
esac
if [ -z "${KEY_PATH:-}" ]; then
    ok "Secure Boot signing not configured — nothing to check"
elif [ -s "$KEY_PATH" ] && python3 - "$KEY_PATH" <<'PY' >/dev/null 2>&1
import sys
from cryptography.hazmat.primitives.serialization import load_pem_private_key
load_pem_private_key(open(sys.argv[1], "rb").read(), password=None)
PY
then
    ok "Signing key valid: $KEY_PATH"
else
    warn "Signing key missing/empty/invalid: ${KEY_PATH:-<unset>}"
    warn "This board has Secure Boot V2 burned into eFuses, so the build must"
    warn "sign with the key that matches the eFuse digest. Put the correct"
    warn ".pem there before building, or the build fails with a PEM error."
fi

# --- 6b. Wi-Fi credentials ---------------------------------------------------
step "Checking Wi-Fi credentials"
WIFI_HDR="$FIRMWARE/secrets/wifi_credentials.h"
WIFI_TMPL="$FIRMWARE/components/config/wifi_credentials.example.h"
if [ -f "$WIFI_HDR" ]; then
    ok "Wi-Fi credentials present: $WIFI_HDR"
elif [ -f "$WIFI_TMPL" ]; then
    mkdir -p "$FIRMWARE/secrets"
    cp "$WIFI_TMPL" "$WIFI_HDR"
    warn "Created $WIFI_HDR from template — EDIT it with your SSID/password"
    warn "(git-ignored; the build uses placeholder values until you do)."
else
    warn "No wifi_credentials.h and no template found — Wi-Fi will not build."
fi

# --- 7. optional build / flash ----------------------------------------------
if [ "$DO_BUILD" -eq 1 ]; then
    step "Building firmware"
    ( cd "$FIRMWARE" && idf.py build )
    ok "Build complete"
fi
if [ "$DO_FLASH" -eq 1 ]; then
    step "Flashing board on $PORT"
    [ -e "$PORT" ] || die "Serial port $PORT not found. Plug in the board or set PORT=..."
    ( cd "$FIRMWARE" && idf.py -p "$PORT" flash )
    ok "Flash complete"
fi

# --- done --------------------------------------------------------------------
echo
echo "${C_GREEN}============================================================${C_RESET}"
ok "Environment ready."
echo
echo "Next steps (in a new terminal):"
echo "  source $IDF_DIR/export.sh"
echo "  cd $FIRMWARE"
echo "  idf.py build                       # compile"
echo "  idf.py -p $PORT flash monitor      # flash + serial log (Ctrl+] to quit)"
echo
echo "Or re-run this script with --build or --flash to do it in one shot."
echo "${C_GREEN}============================================================${C_RESET}"
