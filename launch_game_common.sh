#!/usr/bin/env bash
# ============================================================
# Shared part of launch_game_server.sh and launch_game_client.sh.
# Not meant to be run on its own.
#
# The idea behind it: NO path is hardcoded. The script works out where the
# game is from where the script itself is, so it works on any machine and
# from any folder with nothing to edit. Paths written by hand inside a
# .conf are exactly what breaks when you carry it over to a second machine.
# ============================================================

GAME_ROOT="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"

DEFAULT_PORT=5213
DEFAULT_CYCLES="fixed 30000"

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
grey()  { printf '\033[90m%s\033[0m\n' "$*"; }

error() { red "ERROR: $*"; exit 1; }

# ------------------------------------------------------------
# Checks everything that is needed before launching anything. Every failure
# says exactly what is wrong and how to fix it: that is what turns "it does
# not work" into one concrete line.
# ------------------------------------------------------------
check_environment() {

    command -v dosbox >/dev/null 2>&1 || error \
"DOSBox is not installed.
       Install it with:  sudo apt install dosbox"

    [ -d "$GAME_ROOT/bin" ] || error \
"Cannot find a bin/ folder inside $GAME_ROOT
       The script has to sit in the project root, next to bin/ and res/."

    EXE="$(find "$GAME_ROOT/bin" -maxdepth 1 -iname 'game.exe' | head -1)"
    [ -n "$EXE" ] || error \
"Cannot find bin/GAME.EXE
       Copy the compiled executable to $GAME_ROOT/bin/"

    [ -d "$GAME_ROOT/res" ] || error \
"Cannot find a res/ folder inside $GAME_ROOT
       The game looks for its resources in ..\\res\\ and will not start without it."

    if [ "$PORT" -lt 1024 ]; then
        error \
"Port $PORT is privileged.
       On Linux, anything below 1024 can only be opened by root, so DOSBox's
       tunnel would not start. Use one above 1024 (this script defaults to
       $DEFAULT_PORT)."
    fi
}

# ------------------------------------------------------------
# Writes the DOSBox .conf with the paths already resolved
# ------------------------------------------------------------
generate_conf() {

    local target="$1"
    local ipx_line="$2"
    local log_dir="$3"

    mkdir -p "$(dirname "$target")" "$log_dir"

    cat > "$target" <<CONF_EOF
# Generated automatically by $(basename "${BASH_SOURCE[1]}") on $(date '+%Y-%m-%d %H:%M').
# Do not edit by hand: it is rewritten on every launch.

[dosbox]
machine=svga_s3

[cpu]
core=auto
# Both machines have to run at the same cycles. Lockstep synchronises by
# frame and not by time, so you will not desync from running at different
# speeds, but the faster one would spend half its life waiting for the
# slower one.
cycles=$CYCLES

[sblaster]
sbtype=sb16
sbbase=220
irq=7
dma=1
hdma=5

[ipx]
# DOSBox does not emulate a network card: it emulates IPX directly. No LSL,
# no ODI, no IPXODI, no NET.CFG, no frame types. Nothing to install.
ipx=true

[autoexec]
mount c $GAME_ROOT
# This machine's own K: drive. The game writes its log to k:\\game.log, so two
# copies sharing a K: would overwrite each other's log.
mount k $log_dir
$ipx_line
c:
cd \\bin
game.exe /net
CONF_EOF
}

# ------------------------------------------------------------
# Summary before launching. This is what makes it obvious if you are running
# an old binary, or the project from a different folder.
# ------------------------------------------------------------
summary() {
    grey "--------------------------------------------------------"
    grey "  project    : $GAME_ROOT"
    grey "  executable : $(basename "$EXE")  ($(date -r "$EXE" '+%Y-%m-%d %H:%M'))"
    grey "  conf       : $CONF"
    grey "  log        : $LOG_DIR/GAME.LOG"
    grey "  port       : $PORT/udp"
    grey "  cycles     : $CYCLES"
    grey "--------------------------------------------------------"
}
