#!/usr/bin/env bash
# ============================================================
# Shared part of every launch_game_*.sh script.
# Not meant to be run on its own.
#
# The idea behind it: NO path is hardcoded. The script works out where the
# game is from where the script itself is, so it works on any machine and
# from any folder with nothing to edit. Paths written by hand inside a
# .conf are exactly what breaks when you carry it over to a second machine.
#
#
# THE THREE MODES
# ---------------
#   local     game.exe             two players, one keyboard, one screen
#   net       game.exe /net        two machines, the 320x200 map, one tank each
#   supernet  game.exe /net /bigmap  the same, on the 640x400 map with a camera
#
# Which one you get comes from GAME_ARGS, which each script sets before
# calling generate_conf.
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
       Compile it first, then copy it to $GAME_ROOT/bin/"

    [ -d "$GAME_ROOT/res" ] || error \
"Cannot find a res/ folder inside $GAME_ROOT
       The game looks for its resources in ..\\res\\ and will not start without it."

    # /bigmap needs its two files. They are not in the normal game, so a
    # missing one shows up as a black screen with no explanation.
    if [ "${GAME_ARGS:-}" != "${GAME_ARGS#*bigmap}" ]; then
        for needed in big.bmp bigcol.bmp; do
            [ -f "$GAME_ROOT/res/$needed" ] || error \
"/bigmap needs res/$needed and it is not there.
       Without it the map loads as black and nothing is where it looks."
        done
    fi

    if [ -n "${PORT:-}" ] && [ "$PORT" -lt 1024 ]; then
        error \
"Port $PORT is privileged.
       On Linux, anything below 1024 can only be opened by root, so DOSBox's
       tunnel would not start. Use one above 1024 (this script defaults to
       $DEFAULT_PORT)."
    fi
}

# ------------------------------------------------------------
# Each instance gets its OWN directory to run from, and this is not tidiness.
#
# The game writes its log with fopen("game.log"), a RELATIVE path, so it
# lands in whatever directory DOS is sitting in. Every instance used to run
# from C:\BIN, so two of them on one machine wrote to the same bin/game.log
# and overwrote each other: the loser's log simply did not exist. Mounting a
# separate K: did not help, because nothing ever ran from K:.
#
# The run directory has to be a direct child of the project root, because the
# game asks for its resources as ..\res\ and that has to resolve to the real
# res/. So: <root>/runserv/, <root>/runcli/, <root>/runlocal/.
#
# THE NAME MUST FIT DOS 8.3: at most 8 characters, no dot. This started life
# as run-server, which is 10, and DOS does not refuse it, it MANGLES it: the
# directory turns into RUN-SE~1 and the "cd \run-server" in the autoexec
# stops matching. The check below is here so that never comes back quietly.
#
# The executable is copied in fresh on every launch, which also means the
# summary below is telling you about the binary you are ACTUALLY running.
# ------------------------------------------------------------
prepare_run_dir() {

    local name="$1"

    if [ ${#name} -gt 8 ] || [ "$name" != "${name%%.*}" ]; then
        error "Run directory '$name' does not fit DOS 8.3.
       At most 8 characters and no dot, or DOS mangles it into something
       like ${name:0:6}~1 and the 'cd' in the autoexec no longer finds it."
    fi

    RUN_NAME="$name"
    RUN_DIR="$GAME_ROOT/$RUN_NAME"

    mkdir -p "$RUN_DIR"
    cp -f "$EXE" "$RUN_DIR/GAME.EXE"

    # Start each run without the previous run's log, so what you read is
    # always this run and never half of the last one.
    rm -f "$RUN_DIR/GAME.LOG" "$RUN_DIR/game.log"

    LOG_FILE="$RUN_DIR/GAME.LOG"
}

# ------------------------------------------------------------
# Writes the DOSBox .conf with the paths already resolved.
#
#   $1  where to write it
#   $2  the ipxnet line, or empty for a game with no network at all
#   $3  the run directory name, relative to the root (run-server, run-local...)
# ------------------------------------------------------------
generate_conf() {

    local target="$1"
    local ipx_line="$2"
    local run_name="$3"
    local ipx_enabled="true"

    if [ -z "$ipx_line" ]; then
        ipx_enabled="false"
    fi

    mkdir -p "$(dirname "$target")"

    cat > "$target" <<CONF_EOF
# Generated automatically by $(basename "${BASH_SOURCE[1]}") on $(date '+%Y-%m-%d %H:%M').
# Do not edit by hand: it is rewritten on every launch.
#
# Mode: $MODE_NAME        command line: game.exe $GAME_ARGS

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
ipx=$ipx_enabled

[autoexec]
mount c $GAME_ROOT
$ipx_line
c:
# Not \bin: each instance runs from its own directory so the two of them do
# not overwrite each other's game.log. See prepare_run_dir().
cd \\$run_name
game.exe $GAME_ARGS
CONF_EOF
}

# ------------------------------------------------------------
# Summary before launching. This is what makes it obvious if you are running
# an old binary, or the project from a different folder.
# ------------------------------------------------------------
summary() {
    grey "--------------------------------------------------------"
    grey "  mode       : $MODE_NAME"
    grey "  command    : game.exe $GAME_ARGS"
    grey "  project    : $GAME_ROOT"
    grey "  executable : $(basename "$EXE")  ($(date -r "$EXE" '+%Y-%m-%d %H:%M'))"
    grey "  conf       : $CONF"
    grey "  log        : $LOG_FILE"
    if [ -n "${PORT:-}" ]; then
    grey "  port       : $PORT/udp"
    fi
    grey "  cycles     : $CYCLES"
    grey "--------------------------------------------------------"
}

# ------------------------------------------------------------
# Both machines have to be in the SAME mode. If one is on /bigmap and the
# other is not, the maps are different sizes, the walls are in different
# places, and the two simulations come apart.
#
# The game catches it (map_width is in the state checksum, so it reports a
# desync instead of going quietly mad) but catching it is not the same as
# not doing it.
# ------------------------------------------------------------
warn_same_mode() {
    grey "  BOTH machines have to run the SAME mode. $MODE_NAME here means"
    grey "  $MODE_NAME there: the maps differ between modes and the game will"
    grey "  report a desync if they do not match."
    grey ""
}
