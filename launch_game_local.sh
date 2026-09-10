#!/usr/bin/env bash
# ============================================================
# MODE: LOCAL
#
# Two players on the same keyboard, one screen, the 320x200 map. No network
# of any kind: DOSBox is started with ipx=false, and the game never touches
# net.c.
#
#   ./launch_game_local.sh
#   ./launch_game_local.sh -y 'max'      (different CPU cycles)
#   ./launch_game_local.sh -c my.conf    (your own .conf, nothing generated)
#
# There is no /bigmap here on purpose. The big map needs a camera, a camera
# can only follow one tank, and on one screen that would leave the second
# player driving blind. See the note in main.c.
# ============================================================
set -u

CONF=""
CYCLES="fixed 30000"

MODE_NAME="local"
GAME_ARGS=""

usage() {
    local code="${1:-0}"
    cat <<END
Usage: $(basename "$0") [-y cycles] [-c file.conf]

  -y cycles   Value for DOSBox's cycles=. Defaults to "fixed 30000".
  -c file     Use this .conf as it is instead of generating one.
  -h          This help.

The other two modes are separate scripts:
  ./launch_game_server.sh  +  ./launch_game_client.sh      net / supernet
  ./launch_game_both.sh                                    both, on this machine
END
    exit "$code"
}

while getopts "y:c:h" option; do
    case "$option" in
        y) CYCLES="$OPTARG" ;;
        c) CONF="$OPTARG" ;;
        h) usage 0 ;;
        *) usage 1 ;;
    esac
done

source "$(dirname "${BASH_SOURCE[0]}")/launch_game_common.sh"

if [ -n "$CONF" ]; then
    [ -f "$CONF" ] || error "No such configuration file: $CONF"
    green "Starting DOSBox with $CONF"
    exec dosbox -conf "$CONF"
fi

check_environment
prepare_run_dir "runlocal"

CONF="$GAME_ROOT/net-test/generated-local.conf"

# No ipxnet line at all: this mode has no network, and passing an empty
# second argument is what turns ipx= off in the generated conf.
generate_conf "$CONF" "" "$RUN_NAME"

summary

green ""
green "  LOCAL game: two players, one keyboard."
green ""
grey "    Player 1   arrow keys,  fire with numpad 5"
grey "    Player 2   W A S D,      fire with G"
grey ""
grey "  Leave with ESC (not by closing the window), or the summary line"
grey "  never gets written to the log."
grey ""

exec dosbox -conf "$CONF"
