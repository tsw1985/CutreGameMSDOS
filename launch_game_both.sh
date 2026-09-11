#!/usr/bin/env bash
# ============================================================
# MODE: NET or SUPERNET, BOTH SIDES ON THIS MACHINE.
#
# Starts two DOSBox windows on this computer and joins them to each other
# over the loopback: one is the tunnel server, the other the client. You get
# both tanks, side by side, without a second machine and without typing an
# IP anywhere.
#
#   ./launch_game_both.sh          net       (320x200 map)
#   ./launch_game_both.sh -b       supernet  (640x400 map, one camera each)
#
# This is the one to use for testing. The real two machine setup is
# launch_game_server.sh + launch_game_client.sh.
#
# Each window runs from its own directory (runserv/, runcli/ -- DOS 8.3, so
# no longer than 8 characters), so the
# two logs are separate files and neither overwrites the other. That is the
# whole reason the run directories exist: the game logs to a relative
# "game.log", so two instances in the same directory keep clobbering it.
# ============================================================
set -u

PORT=5213
CYCLES="fixed 30000"
THEME=""
THEME_CLIENT=""

MODE_NAME="net"
GAME_ARGS="/net"

usage() {
    local code="${1:-0}"
    cat <<END
Usage: $(basename "$0") [-b] [-p port] [-y cycles]

  -b          SUPERNET: the big 640x400 map with a scrolling camera.
              Without it you get NET: the plain 320x200 map.
  -t theme    How the big map looks: sky, war or neon. Needs -b.
  -T theme    Theme for the CLIENT window only, so you can see two looks
              of the same match side by side. Needs -b.
  -p port     UDP port for the loopback tunnel. Defaults to 5213.
  -y cycles   Value for DOSBox's cycles=. Defaults to "fixed 30000".
  -h          This help.

The other modes:
  ./launch_game_local.sh                                   two on one keyboard
  ./launch_game_server.sh  +  ./launch_game_client.sh      two real machines
END
    exit "$code"
}

while getopts "bt:T:p:y:h" option; do
    case "$option" in
        b) MODE_NAME="supernet"; GAME_ARGS="/net /bigmap" ;;
        t) THEME="$OPTARG" ;;
        T) THEME_CLIENT="$OPTARG" ;;
        p) PORT="$OPTARG" ;;
        y) CYCLES="$OPTARG" ;;
        h) usage 0 ;;
        *) usage 1 ;;
    esac
done

source "$(dirname "${BASH_SOURCE[0]}")/launch_game_common.sh"

# -T on its own means "same theme as the server, but only say it once"
if [ -n "$THEME_CLIENT" ] && [ -z "$THEME" ]; then
    THEME="$THEME_CLIENT"
fi

# apply_theme validates and appends -sky/-war/-neon to GAME_ARGS. Keep the
# result as the base for the server, and build the client's separately below.
apply_theme
BASE_ARGS="$GAME_ARGS"

check_environment

if command -v ss >/dev/null 2>&1; then
    if ss -lun 2>/dev/null | grep -q ":$PORT\b"; then
        error "Port $PORT/udp is already in use.
       Close the other copy, or pick another port with -p."
    fi
fi

# ---- the server side ----
GAME_ARGS="$BASE_ARGS"
prepare_run_dir "runserv"
SERVER_RUN="$RUN_NAME"
SERVER_LOG="$LOG_FILE"
SERVER_CONF="$GAME_ROOT/net-test/generated-both-server.conf"
generate_conf "$SERVER_CONF" "ipxnet startserver $PORT" "$SERVER_RUN"

# ---- the client side ----
#
# It can wear a DIFFERENT theme from the server, with -T. That is not a
# gimmick: it is the clearest demonstration that a theme is pure decoration.
# The two windows show the same match, frame for frame, in two different
# paint jobs, and neither desyncs, because the walls come from bigcol.bmp on
# both sides.
GAME_ARGS="$BASE_ARGS"
if [ -n "$THEME_CLIENT" ] && [ "$THEME_CLIENT" != "$THEME" ]; then

    case "$THEME_CLIENT" in
        sky|war|neon) ;;
        *) error "Unknown client theme '$THEME_CLIENT'.
       It has to be sky, war or neon." ;;
    esac

    if [ "$MODE_NAME" != "supernet" ]; then
        error "-T only dresses the big map, so it needs -b as well."
    fi

    # swap the server's theme flag for the client's
    GAME_ARGS="${BASE_ARGS% -*} -$THEME_CLIENT"

fi

prepare_run_dir "runcli"
CLIENT_RUN="$RUN_NAME"
CLIENT_LOG="$LOG_FILE"
CLIENT_CONF="$GAME_ROOT/net-test/generated-both-client.conf"
generate_conf "$CLIENT_CONF" "ipxnet connect 127.0.0.1 $PORT" "$CLIENT_RUN"

grey "--------------------------------------------------------"
grey "  mode       : $MODE_NAME   (both sides on this machine)"
grey "  server     : game.exe $BASE_ARGS"
grey "  client     : game.exe $GAME_ARGS"
grey "  project    : $GAME_ROOT"
grey "  executable : $(basename "$EXE")  ($(date -r "$EXE" '+%Y-%m-%d %H:%M'))"
grey "  port       : $PORT/udp on loopback"
grey "  cycles     : $CYCLES"
grey "  server log : $SERVER_LOG"
grey "  client log : $CLIENT_LOG"
grey "--------------------------------------------------------"

# If either window is closed or crashes, take the other one down with it
# instead of leaving an orphan DOSBox holding the port.
SERVER_PID=""
CLIENT_PID=""
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null
    [ -n "$CLIENT_PID" ] && kill "$CLIENT_PID" 2>/dev/null
    return 0
}
trap cleanup EXIT INT TERM

green ""
green "  Starting the SERVER window..."

# DOSBox is SDL 1, which reads its window position out of the environment.
# Without this the second window lands exactly on top of the first one.
SDL_VIDEO_WINDOW_POS="40,80" dosbox -conf "$SERVER_CONF" >/dev/null 2>&1 &
SERVER_PID=$!

# The tunnel has to be listening before the client tries to reach it, or the
# client gives up with "Timeout connecting to server" and the game sits there
# searching for an opponent that never announces itself.
sleep 3

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    error "The server window died on startup.
       Run ./launch_game_server.sh on its own to see what it says."
fi

green "  Starting the CLIENT window..."
green ""

SDL_VIDEO_WINDOW_POS="720,80" dosbox -conf "$CLIENT_CONF" >/dev/null 2>&1 &
CLIENT_PID=$!

grey "  Two windows. Click one to give it the keyboard: each window drives"
grey "  its own tank, and in $MODE_NAME each one has its own view of the map."
grey ""
grey "    Arrow keys to move, numpad 5 to fire, ESC to quit."
grey ""
grey "  Leave with ESC in BOTH windows, not by closing them: the summary"
grey "  line only gets written to the log on a clean exit."
grey ""
grey "  When you are done, read the logs with:"
grey "      cat $SERVER_LOG"
grey "      cat $CLIENT_LOG"
grey ""

wait "$SERVER_PID" 2>/dev/null
wait "$CLIENT_PID" 2>/dev/null

trap - EXIT INT TERM

green ""
green "  Both windows closed."
green ""
for pair in "SERVER:$SERVER_LOG" "CLIENT:$CLIENT_LOG"; do
    role="${pair%%:*}"
    file="${pair#*:}"
    if [ -f "$file" ]; then
        grey "  --- $role: memory and mode ---"
        grep -E "Memory at start|memory now|could not load|Sound:|desync|Desync" "$file" 2>/dev/null | sed 's/^/      /'
    else
        red "  $role wrote no log at $file"
    fi
done
grey ""
