#!/usr/bin/env bash
# ============================================================
# Starts the game over the network, connecting to the SERVER on the other
# machine.
#
# Start this one AFTER the server: if the tunnel is not up, DOSBox just says
# "Timeout connecting to server" and the game sits there searching.
#
#   ./launch_game_client.sh 192.168.1.45
#   ./launch_game_client.sh 192.168.1.45 -p 6000
#   ./launch_game_client.sh -c my.conf
# ============================================================
set -u

PORT=5213
CONF=""
CYCLES="fixed 30000"
SERVER=""

usage() {
    local code="${1:-0}"
    cat <<END
Usage: $(basename "$0") <server-ip> [-p port] [-y cycles]
       $(basename "$0") -c file.conf

  <ip>        IP of the machine that ran launch_game_server.sh.
              That script tells you the IP when it starts.
  -p port     UDP port for the tunnel. Defaults to 5213.
              Must be the SAME one the server uses.
  -y cycles   Value for DOSBox's cycles=. Defaults to "fixed 30000".
              Must be the SAME on both machines.
  -c file     Use this .conf as it is instead of generating one.
  -h          This help.
END
    exit "$code"
}

# The IP comes on its own, with no letter in front, so it is taken before getopts
if [ $# -gt 0 ] && [ "${1:0:1}" != "-" ]; then
    SERVER="$1"
    shift
fi

while getopts "p:c:y:h" option; do
    case "$option" in
        p) PORT="$OPTARG" ;;
        c) CONF="$OPTARG" ;;
        y) CYCLES="$OPTARG" ;;
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

if [ -z "$SERVER" ]; then
    red "Missing the server's IP."
    echo
    usage 1
fi

check_environment

# If it does not even ping, the problem is the network and not the game. Worth
# knowing BEFORE the screen switches to graphics mode and nothing can be read.
if command -v ping >/dev/null 2>&1; then
    if ! ping -c 1 -W 2 "$SERVER" >/dev/null 2>&1; then
        red "WARNING: $SERVER does not answer to ping."
        grey "  That is usually the cable, a mistyped IP, or the server's"
        grey "  firewall. If ping does not work, the game will not either."
        grey ""
        printf "  Carry on anyway? [y/N] "
        read -r answer
        case "$answer" in
            y|Y|s|S) ;;
            *) exit 1 ;;
        esac
    fi
fi

LOG_DIR="$GAME_ROOT/net-test/log-client"
CONF="$GAME_ROOT/net-test/generated-client.conf"

generate_conf "$CONF" "ipxnet connect $SERVER $PORT" "$LOG_DIR"

summary

green ""
green "  CLIENT connecting to $SERVER:$PORT"
green ""
grey "  If you get 'Timeout connecting to server':"
grey "    - the server is not running yet, or"
grey "    - its firewall is blocking the port:  sudo ufw allow $PORT/udp"
grey ""
grey "  Leave the game with ESC (not by closing the window), or the summary"
grey "  line never gets written to the log."
grey ""

exec dosbox -conf "$CONF"
