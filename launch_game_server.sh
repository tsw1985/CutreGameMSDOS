#!/usr/bin/env bash
# ============================================================
# Starts the game over the network, acting as the tunnel SERVER.
#
# This is the machine to start FIRST. It brings up the IPX tunnel and joins
# it as a player: it plays too, it is not a separate process.
#
#   ./launch_game_server.sh                    (port 5213)
#   ./launch_game_server.sh -p 6000            (another port)
#   ./launch_game_server.sh -c my.conf         (your own .conf, nothing generated)
#   ./launch_game_server.sh -y 'max'           (different CPU cycles)
#
# The script tells you the IP to give the other player.
# ============================================================
set -u

PORT=5213
CONF=""
CYCLES="fixed 30000"

usage() {
    # The exit code is passed in: 0 when the user asked for help, 1 when the
    # help is shown because the script was called wrongly.
    local code="${1:-0}"
    cat <<END
Usage: $(basename "$0") [-p port] [-c file.conf] [-y cycles]

  -p port     UDP port for the tunnel. Defaults to 5213.
              Must be the SAME one the client uses, and above 1024.
  -c file     Use this .conf as it is instead of generating one.
  -y cycles   Value for DOSBox's cycles=. Defaults to "fixed 30000".
              Must be the SAME on both machines.
  -h          This help.
END
    exit "$code"
}

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

check_environment

LOG_DIR="$GAME_ROOT/net-test/log-server"
CONF="$GAME_ROOT/net-test/generated-server.conf"

generate_conf "$CONF" "ipxnet startserver $PORT" "$LOG_DIR"

# The port has to be free: if another copy is holding it the server does not
# start, and all the client sees is an unexplained "Timeout".
if command -v ss >/dev/null 2>&1; then
    if ss -lun 2>/dev/null | grep -q ":$PORT\b"; then
        error "Port $PORT/udp is already in use.
       Close the other copy, or pick another port with -p."
    fi
fi

summary

# The IP the other player needs. Always shown, because it is the one thing
# that has to be carried over to the other machine.
IPS="$(ip -4 -o addr show scope global 2>/dev/null | awk '{split($4,a,"/"); print a[1]}')"

green ""
green "  SERVER ready. On the OTHER machine run:"
green ""
for ip in $IPS; do
    green "      ./launch_game_client.sh $ip -p $PORT"
done
green ""

# The firewall is the most common failure: the client only sees a "Timeout".
if command -v ufw >/dev/null 2>&1; then
    if ! ufw status 2>/dev/null | grep -q "$PORT/udp"; then
        grey "  If the other player gets stuck on 'Timeout connecting to server',"
        grey "  open THIS machine's firewall:"
        grey "      sudo ufw allow $PORT/udp"
        grey ""
    fi
fi

grey "  Leave the game with ESC (not by closing the window), or the summary"
grey "  line never gets written to the log."
grey ""

exec dosbox -conf "$CONF"
