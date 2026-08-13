#!/usr/bin/env bash
#
# setup_env.sh — Bootstrap the full build/flash environment for SentinelOS.
#
# After cloning the repo, run this once on any PC to get everything needed to
# build and flash the board: ESP-IDF, the Xtensa toolchain, cmake/ninja, the
# board submodule, Wi-Fi credentials, and the signing key. Idempotent — safe to
# re-run; it skips steps already done.
#
# Usage:
#   tools/setup_env.sh                  # set up the environment
#   tools/setup_env.sh --build          # ...then build the firmware
#   tools/setup_env.sh --flash          # ...then build and flash the board
#   tools/setup_env.sh --wifi           # (re)configure Wi-Fi credentials
#   tools/setup_env.sh --gen-signing-key  # generate a fresh Secure Boot key
#
# Configurable via environment variables:
#   IDF_BRANCH     ESP-IDF branch/tag to install   (default: release/v6.0)
#   IDF_DIR        Where ESP-IDF is installed       (default: ~/esp/esp-idf-v6)
#   IDF_TARGET     Chip target                      (default: esp32)
#   PORT           Serial port for flashing         (default: /dev/ttyUSB0)
#   WIFI_SSID      Wi-Fi SSID (non-interactive)     (prompts if unset)
#   WIFI_PASSWORD  Wi-Fi password (non-interactive) (prompts if unset)
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
DO_WIFI=0
DO_GENKEY=0
for arg in "$@"; do
    case "$arg" in
        --build) DO_BUILD=1 ;;
        --flash) DO_BUILD=1; DO_FLASH=1 ;;
        --wifi) DO_WIFI=1 ;;
        --gen-signing-key) DO_GENKEY=1 ;;
        -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "Unknown argument: $arg (use --build, --flash, --wifi, --gen-signing-key, or --help)" ;;
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
key_is_valid() {
    [ -s "$1" ] && python3 - "$1" <<'PY' >/dev/null 2>&1
import sys
from cryptography.hazmat.primitives.serialization import load_pem_private_key
load_pem_private_key(open(sys.argv[1], "rb").read(), password=None)
PY
}
if [ -z "${KEY_PATH:-}" ]; then
    ok "Secure Boot signing not configured — nothing to check"
elif key_is_valid "$KEY_PATH"; then
    ok "Signing key valid: $KEY_PATH"
elif [ "$DO_GENKEY" -eq 1 ]; then
    warn "Generating a NEW RSA-3072 Secure Boot signing key at $KEY_PATH"
    warn "Only use this on a board NOT yet secure-boot-provisioned (its digest"
    warn "will be burned on first boot). A board already provisioned with a"
    warn "different key will refuse to boot images signed by this one."
    mkdir -p "$(dirname "$KEY_PATH")"
    ( cd "$FIRMWARE" && idf.py secure-generate-signing-key --scheme rsa3072 "$KEY_PATH" )
    key_is_valid "$KEY_PATH" && ok "Generated $KEY_PATH" || die "key generation failed"
else
    warn "Signing key missing/empty/invalid: $KEY_PATH"
    warn "Provision it before building. Either:"
    warn "  - copy the key that matches this board's burned eFuse digest, or"
    warn "  - for a fresh board, re-run with --gen-signing-key to create one."
fi

# --- 6b. Wi-Fi credentials ---------------------------------------------------
step "Configuring Wi-Fi credentials"
WIFI_HDR="$FIRMWARE/secrets/wifi_credentials.h"
WIFI_TMPL="$FIRMWARE/components/config/wifi_credentials.example.h"

# Escape backslashes and double-quotes so odd passwords stay valid C strings.
wifi_escape() { printf '%s' "$1" | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'; }

write_wifi_header() {
    mkdir -p "$FIRMWARE/secrets"
    {
        echo "#ifndef WIFI_CREDENTIALS_H"
        echo "#define WIFI_CREDENTIALS_H"
        echo ""
        echo "/* Local Wi-Fi credentials — generated by tools/setup_env.sh, git-ignored. */"
        echo ""
        echo "#define SENTINELOS_WIFI_SSID     \"$(wifi_escape "$1")\""
        echo "#define SENTINELOS_WIFI_PASSWORD \"$(wifi_escape "$2")\""
        echo ""
        echo "#endif /* WIFI_CREDENTIALS_H */"
    } > "$WIFI_HDR"
    chmod 600 "$WIFI_HDR"
}

current_ssid() {
    sed -nE 's/.*SENTINELOS_WIFI_SSID[[:space:]]+"([^"]*)".*/\1/p' "$WIFI_HDR" 2>/dev/null | head -1
}

if [ -n "${WIFI_SSID:-}" ] && [ -n "${WIFI_PASSWORD:-}" ]; then
    # Non-interactive: values supplied via environment.
    write_wifi_header "$WIFI_SSID" "$WIFI_PASSWORD"
    ok "Wrote $WIFI_HDR from WIFI_SSID/WIFI_PASSWORD"
elif [ -f "$WIFI_HDR" ] && [ "$DO_WIFI" -eq 0 ]; then
    ok "Wi-Fi credentials present: $WIFI_HDR (use --wifi to reconfigure)"
elif [ -t 0 ]; then
    # Interactive prompt (password input is hidden).
    cur="$(current_ssid)"
    printf "    Wi-Fi SSID%s: " "${cur:+ [$cur]}"
    read -r in_ssid
    in_ssid="${in_ssid:-$cur}"
    printf "    Wi-Fi password: "
    read -rs in_pass; echo
    if [ -z "$in_ssid" ]; then
        warn "No SSID entered — leaving Wi-Fi credentials unchanged."
    else
        write_wifi_header "$in_ssid" "$in_pass"
        ok "Wrote $WIFI_HDR"
    fi
elif [ -f "$WIFI_HDR" ]; then
    ok "Wi-Fi credentials present: $WIFI_HDR"
elif [ -f "$WIFI_TMPL" ]; then
    mkdir -p "$FIRMWARE/secrets"
    cp "$WIFI_TMPL" "$WIFI_HDR"
    warn "No credentials given and not a terminal — wrote placeholder $WIFI_HDR."
    warn "Set WIFI_SSID/WIFI_PASSWORD or run with --wifi, then rebuild."
else
    warn "No wifi_credentials.h, no template, and no credentials given — Wi-Fi will not build."
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
