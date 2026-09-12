#!/usr/bin/env bash
# ============================================================
# CutreGame - the one script for playing.
#
#   ./play.sh                              the help
#   ./play.sh local                        two players, one keyboard
#   ./play.sh both                         two windows here, small map
#   ./play.sh both   -b -t neon -l 3       two windows here, big map
#   ./play.sh server -b -t war  -l 3       machine 1 of 2
#   ./play.sh client <ip> -b -t war -l 3   machine 2 of 2
#
# This used to be five files. It is one because the four modes share almost
# everything: the same checks, the same .conf, the same working directory.
# All that changes is the ipxnet line and how many windows are opened.
#
# NOTHING in here carries a hand written path: it all comes from where this
# file is, so it works on any machine and from any folder. A path typed by
# hand into a .conf is exactly what breaks when you take it to the second
# machine.
# ============================================================
set -u

GAME_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PORT=5213

# ------------------------------------------------------------
# HOW MUCH POWER DOSBOX GETS
#
# 'dynamic' recompiles the 16 bit code into native host code instead of
# interpreting it instruction by instruction. It is several times faster
# than the 'auto' that was here before and changes nothing about what the
# program does.
#
# 'max' lets DOSBox use a whole core of your PC. It used to be
# "fixed 30000", which is a weak 386DX-40: the intro effects that work pixel
# by pixel crawled along at 8 or 9 frames per second.
#
# And raising the cycles HERE is safe, which is the thing that usually is
# not safe on DOS. The whole program waits for the vertical retrace: the
# game loop calls wait_retrace() and every demo goes through demo_show(),
# which does the same. The ceiling is ~70 frames per second whatever
# happens, so more power speeds nothing up, it only leaves each frame with
# time to spare instead of running short. The classic "on a fast PC the game
# runs at a thousand miles an hour" cannot happen here.
#
# If the sound stutters, which can happen when your Linux is loaded, go back
# to a fixed number:   -y "fixed 100000"
# ------------------------------------------------------------
CORE="dynamic"
CYCLES="max"
THEME=""
THEME_CLIENT=""
LEVEL=""
SERVER_IP=""
DEMO=0
DEMO_FOUND=0

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
grey()  { printf '\033[90m%s\033[0m\n' "$*"; }
error() { red "ERROR: $*"; exit 1; }


# ------------------------------------------------------------
usage() {
cat <<'END'

  CutreGame - how to play

  ONE MACHINE
    ./play.sh local                      two players on one keyboard
    ./play.sh both                       two windows, small map
    ./play.sh both -b                    two windows, big map
    ./play.sh both -b -t neon -l 3       ... with a theme and a level
    ./play.sh local -demo                with the intro first

  TWO MACHINES
    ./play.sh server -b -t war -l 3           on the first one
    ./play.sh client 192.168.1.45 -b -t war -l 3   on the second one

    ALWAYS start the server first. It tells you the IP to type on the
    other machine.

  OPTIONS
    -b            big 640x400 map with a camera (without it, 320x200)
    -t THEME      sky, war or neon.  Needs -b
    -l N          level 1 to 5.      Needs -b
    -T THEME      only in 'both': theme of the SECOND window
    -d, -demo     the intro: 15 pictures with effects before you play
                  (in 'both' only window 1 shows it)
    -p PORT       UDP port of the tunnel. Default 5213
    -y CYCLES     DOSBox cycles=. Default "max" (all of your PC)
    -c CORE       DOSBox core=.   Default "dynamic"

  WHAT HAS TO MATCH BETWEEN THE TWO MACHINES
    -b   YES. Maps of different sizes = different games.
    -l   YES. Every level has different walls.
    -t   NO.  It is only paint: the three themes share the walls.

    If the -l do not match the game negotiates it instead of breaking, but
    PLAYER 1 wins, and player 1 is drawn at random in every match and is
    NOT whoever started the server. So pass the same one on both.

  THE INTRO  (-d)
    Shows res/demo/demo01.bmp .. demo15.bmp, each one through a different
    effect, 8 seconds each, with the music. ESC skips it and the match
    begins.

    The pictures have to be 320x200 BMPs with 256 COLOURS, 65078 bytes
    each. One that is missing is simply skipped.

    Over a network the two machines AGREE who shows it: whoever was
    started with -demo does, and the other one waits with a message on
    screen until it is over. If both were started with it, player 1 shows
    it. ESC skips it from EITHER keyboard, including the one that is only
    watching a "waiting" message.

    In 'both' only window 1 gets -demo, because watching the same intro
    twice on one screen would be daft.

  IF IT IS SLOW, OR IF IT IS ODD
    By default DOSBox runs at core=dynamic and cycles=max, that is, all
    your PC can give. Nothing can run "too fast": the game and the demos
    wait for the vertical retrace and are capped at about 70 fps.

    If the SOUND stutters, pin the cycles:
      ./play.sh local -demo -y "fixed 100000"

    If something behaves oddly, go back to the old slow interpreter:
      ./play.sh local -c auto -y "fixed 30000"

  CONTROLS
    local     player 1: arrows + 5 on the numeric keypad
              player 2: W A S D + G
    network   both with arrows + 5 on the numeric keypad
    ESC to quit (do not close the window: the log is left half written)

END
exit "${1:-0}"
}


# ------------------------------------------------------------
# The mode is the first word, and the IP the second one if the mode is client.
# ------------------------------------------------------------
MODE="${1:-}"
[ -z "$MODE" ] && usage 0
shift

case "$MODE" in
    local|both|server|client) ;;
    -h|--help|help)           usage 0 ;;
    *) red "Unknown mode: '$MODE'"; usage 1 ;;
esac

if [ "$MODE" = "client" ]; then
    if [ $# -gt 0 ] && [ "${1:0:1}" != "-" ]; then
        SERVER_IP="$1"
        shift
    fi
fi

MODE_NAME="$MODE"
GAME_ARGS=""

# ------------------------------------------------------------
# The role goes to the game, and it is used for exactly one thing: deciding
# which machine shows the intro.
#
# This script is the only thing that knows. It is the one writing
# "ipxnet startserver" into one .conf and "ipxnet connect" into the other;
# game.exe never sees any of that, and once the match is running there is no
# server and no client, only two peers.
#
# 'both' is two windows on this machine, so window 1 is the server and window
# 2 the client, set further down where the two argument lists are built.
# ------------------------------------------------------------
[ "$MODE" != "local" ] && GAME_ARGS="/net"
[ "$MODE" = "server" ] && GAME_ARGS="$GAME_ARGS /server"
[ "$MODE" = "client" ] && GAME_ARGS="$GAME_ARGS /client"

# ------------------------------------------------------------
# getopts only understands ONE letter options: hand it -demo and it reads
# -d -e -m -o. Since the game itself is called with "-demo" and that is what
# anybody types without thinking, it is pulled out of the list first and both
# spellings are accepted.
# ------------------------------------------------------------
FILTERED=()
for argument in "$@"; do
    case "$argument" in
        -demo|--demo|/demo) DEMO=1 ;;
        *)                  FILTERED+=("$argument") ;;
    esac
done
if [ ${#FILTERED[@]} -gt 0 ]; then
    set -- "${FILTERED[@]}"
else
    set --
fi

while getopts "bdt:T:l:p:y:c:h" option; do
    case "$option" in
        b) GAME_ARGS="$GAME_ARGS /bigmap" ;;
        d) DEMO=1 ;;
        t) THEME="$OPTARG" ;;
        T) THEME_CLIENT="$OPTARG" ;;
        l) LEVEL="$OPTARG" ;;
        p) PORT="$OPTARG" ;;
        y) CYCLES="$OPTARG" ;;
        c) CORE="$OPTARG" ;;
        h) usage 0 ;;
        *) usage 1 ;;
    esac
done

BIGMAP=0
[ "$GAME_ARGS" != "${GAME_ARGS#*bigmap}" ] && BIGMAP=1


# ------------------------------------------------------------
# The theme only dresses the big map, and the level only exists there.
#
# The theme is pure decoration: the three of them share the collision map byte
# for byte, so the two machines can wear different ones. The level cannot: each
# one has its own walls.
# ------------------------------------------------------------
if [ -n "$THEME" ]; then

    case "$THEME" in
        sky|war|neon) ;;
        *) error "Unknown theme '$THEME'. It has to be sky, war or neon." ;;
    esac

    [ "$BIGMAP" = "0" ] && error "-t needs -b: themes only dress the big map."

    GAME_ARGS="$GAME_ARGS -$THEME"

fi

if [ -n "$LEVEL" ]; then

    case "$LEVEL" in
        1|2|3|4|5) ;;
        *) error "Unknown level '$LEVEL'. It has to be 1 to 5." ;;
    esac

    [ "$BIGMAP" = "0" ] && error "-l needs -b: levels only exist on the big map."

    # Levels only come dressed, there is no res/15Level/ORIGINAL. The game does
    # the same on its own; doing it here as well is what lets the check further
    # down make sure the files are there.
    if [ -z "$THEME" ]; then
        THEME="sky"
        GAME_ARGS="$GAME_ARGS -sky"
        grey "  (levels only come dressed: level $LEVEL is using -sky)"
    fi

    GAME_ARGS="$GAME_ARGS -level$LEVEL"

fi

if [ -n "$THEME_CLIENT" ]; then
    [ "$MODE" != "both" ] && error "-T only makes sense in 'both'."
    case "$THEME_CLIENT" in
        sky|war|neon) ;;
        *) error "Unknown theme '$THEME_CLIENT'." ;;
    esac
fi

[ "$MODE" = "client" ] && [ -z "$SERVER_IP" ] && \
    { red "The server IP is missing."; usage 1; }

[ "$MODE" != "local" ] && [ "$PORT" -lt 1024 ] && \
    error "Port $PORT is privileged: on Linux only root can open it. Use one above 1024."


# ------------------------------------------------------------
# Everything that is needed, checked before anything is opened. Every failure
# says exactly what is wrong and how to fix it: that is what turns an "it does
# not work" into one concrete line.
# ------------------------------------------------------------
command -v dosbox >/dev/null 2>&1 || error \
"DOSBox is not installed.
       Install it with:  sudo apt install dosbox"

EXE="$(find "$GAME_ROOT/bin" -maxdepth 1 -iname 'game.exe' 2>/dev/null | head -1)"
[ -n "$EXE" ] || error \
"I cannot find bin/GAME.EXE
       Build it first and copy it to $GAME_ROOT/bin/"

[ -d "$GAME_ROOT/res" ] || error \
"I cannot find the res/ folder inside $GAME_ROOT
       The game looks for its resources in ..\\res\\ and will not start without it."

if [ "$BIGMAP" = "1" ]; then

    case "$THEME" in
        sky)  spr=spr_sky.bmp;  folder=SKYNET  ;;
        war)  spr=spr_war.bmp;  folder=MILITAR ;;
        neon) spr=spr_neon.bmp; folder=NEON    ;;
        *)    spr=sprites.bmp;  folder=SKYNET  ;;
    esac

    [ -f "$GAME_ROOT/res/$spr" ] || error "res/$spr is missing (the theme's sprite sheet)."

    if [ -z "$LEVEL" ]; then

        case "$THEME" in
            sky)  map=map_sky.bmp  ;;
            war)  map=map_war.bmp  ;;
            neon) map=map_neon.bmp ;;
            *)    map=big.bmp      ;;
        esac

        [ -f "$GAME_ROOT/res/$map" ]        || error "res/$map is missing"
        [ -f "$GAME_ROOT/res/bigcol.bmp" ]  || error "res/bigcol.bmp is missing (the collision map)."

    else

        # Every level brings ITS OWN bigcol.bmp, and that is why it has to be
        # agreed over the network: a different level is different walls.
        dir="$GAME_ROOT/res/15Level/$folder/NIVEL0$LEVEL"

        [ -d "$dir" ] || error \
"I cannot find $dir
       Levels 1 to 5 live in res/15Level/<THEME>/NIVELnn/ and each one
       brings its own big.bmp and bigcol.bmp."

        for f in big.bmp bigcol.bmp; do
            [ -f "$dir/$f" ] || error "Level $LEVEL of theme '$THEME' is missing $f
       I looked for it in $dir"
        done

    fi

fi


# ------------------------------------------------------------
# THE INTRO
#
# It goes last on the line because that reads well, and the game does not care
# what order its arguments come in.
#
# The pictures are checked here for the same reason the levels are: the game
# silently skips any it cannot find, so without this an intro with all fifteen
# in the wrong place would be two minutes of black screen with nobody telling
# you why.
# ------------------------------------------------------------
if [ "$DEMO" = "1" ]; then

    # ------------------------------------------------------------
    # The intro is shown by the SERVER. Always.
    #
    # Not a matter of taste: the client is the end that waits, and letting
    # both ends carry -demo is what produced two machines each staring at a
    # "the other one is showing the intro" message. Refusing it here is one
    # line and it makes the rule impossible to get wrong.
    # ------------------------------------------------------------
    [ "$MODE" = "client" ] && error \
"The client does not show the intro, the server does.
       Put -demo on the machine you start with 'play.sh server' and launch
       this one without it: it waits for the intro to finish on its own,
       however late you start it."

    GAME_ARGS="$GAME_ARGS -demo"

    [ -d "$GAME_ROOT/res/demo" ] || error \
"I cannot find the res/demo/ folder
       The intro looks there for res/demo/demo01.bmp .. demo15.bmp"

    DEMO_FOUND=0
    DEMO_BAD=""

    for n in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15; do

        img="$GAME_ROOT/res/demo/demo$n.bmp"
        [ -f "$img" ] || continue

        DEMO_FOUND=$((DEMO_FOUND + 1))

        # 65078 bytes exactly: 54 of header + 1024 of palette + 320*200.
        # A 256 colour BMP that uses fewer colours comes out shorter, and the
        # game does a bare fseek(1078): the picture would come out skewed
        # diagonally instead of giving an error.
        size=$(stat -c %s "$img")
        [ "$size" = "65078" ] || DEMO_BAD="$DEMO_BAD demo$n.bmp($size)"

    done

    [ "$DEMO_FOUND" -gt 0 ] || error \
"There is not one picture in res/demo/.
       They have to be called demo01.bmp .. demo15.bmp, be 320x200 BMPs with
       256 colours, and be exactly 65078 bytes."

    if [ -n "$DEMO_BAD" ]; then
        red "WARNING: these are not 65078 bytes and will come out skewed:"
        red "      $DEMO_BAD"
        grey "  Save them as 320x200 BMPs with the full 256 entry palette."
    fi

fi


# ------------------------------------------------------------
# Each instance runs from ITS OWN directory, and it is not about tidiness.
#
# The game writes its log with fopen("game.log"), a RELATIVE path, so it lands
# in whatever directory DOS is in. If both instances ran from the same place
# they would overwrite each other's log and one of them simply would not exist.
#
# It has to hang off the project root, because the game asks for its resources
# as ..\res\ and that has to reach the real res/.
#
# And the name has to fit DOS 8.3: 8 characters, no dot. Any longer and DOS
# does not reject it, it MANGLES it (runserver -> RUNSER~1) and the cd in the
# autoexec stops finding it.
# ------------------------------------------------------------
prepare_run_dir() {

    local name="$1"

    [ ${#name} -gt 8 ] && error "The directory '$name' does not fit DOS 8.3."

    RUN_DIR="$GAME_ROOT/$name"
    mkdir -p "$RUN_DIR"
    cp -f "$EXE" "$RUN_DIR/GAME.EXE"
    rm -f "$RUN_DIR/GAME.LOG" "$RUN_DIR/game.log"

    RUN_NAME="$name"
    LOG_FILE="$RUN_DIR/GAME.LOG"

}


# ------------------------------------------------------------
# Writes the DOSBox .conf with every path already resolved.
#   $1 target file   $2 ipxnet line (empty = no network)
#   $3 working directory   $4 game arguments
# ------------------------------------------------------------
generate_conf() {

    local target="$1" ipx_line="$2" run_name="$3" args="$4"
    local ipx_enabled="true"

    [ -z "$ipx_line" ] && ipx_enabled="false"

    mkdir -p "$(dirname "$target")"

    cat > "$target" <<CONF_EOF
# Generated by play.sh on $(date '+%Y-%m-%d %H:%M'). Do not edit: it gets rewritten.
# Mode: $MODE_NAME    line: game.exe $args

[dosbox]
machine=svga_s3

[cpu]
# dynamic recompiles to native code instead of interpreting. max gives it a
# whole core of the machine.
#
# The two machines do not have to run at the same speed: the lockstep syncs by
# frame, and on top of that both ends are capped by the vertical retrace at
# about 70 fps, so once both reach that ceiling they run just as fast even if
# one of them is twice the machine.
core=$CORE
cycles=$CYCLES

[sblaster]
sbtype=sb16
sbbase=220
irq=7
dma=1
hdma=5

[ipx]
# DOSBox does not emulate a network card: it emulates IPX directly. No LSL, no
# ODI, no IPXODI, no NET.CFG and no frame types. Nothing to install.
ipx=$ipx_enabled

[autoexec]
mount c $GAME_ROOT
$ipx_line
c:
cd \\$run_name
game.exe $args
CONF_EOF

}


summary() {
    grey "--------------------------------------------------------"
    grey "  mode       : $MODE_NAME"
    [ "$BIGMAP" = "1" ] && grey "  theme      : ${THEME:-original}"
    [ "$BIGMAP" = "1" ] && grey "  level      : ${LEVEL:-0}   (0 = the original big map)"
    [ "$DEMO" = "1" ]   && grey "  intro      : yes, $DEMO_FOUND pictures from res/demo/  (ESC skips it)"
    grey "  line       : game.exe $GAME_ARGS"
    grey "  executable : $(date -r "$EXE" '+%Y-%m-%d %H:%M')"
    grey "  dosbox     : core=$CORE  cycles=$CYCLES"
    [ "$MODE" != "local" ] && grey "  port       : $PORT/udp"
    grey "--------------------------------------------------------"
}


CONF_DIR="$GAME_ROOT/net-test"


# ============================================================
#  LOCAL
# ============================================================
if [ "$MODE" = "local" ]; then

    prepare_run_dir "runlocal"
    CONF="$CONF_DIR/generated-local.conf"
    generate_conf "$CONF" "" "$RUN_NAME" "$GAME_ARGS"

    summary
    green ""
    green "  Two players on one keyboard."
    green ""
    grey  "    player 1   arrows, fire with 5 on the numeric keypad"
    grey  "    player 2   W A S D, fire with G"
    grey  ""
    grey  "  Quit with ESC, not by closing the window."
    grey  ""

    exec dosbox -conf "$CONF"

fi


# ============================================================
#  BOTH - both windows on this machine
# ============================================================
if [ "$MODE" = "both" ]; then

    if command -v ss >/dev/null 2>&1 && ss -lun 2>/dev/null | grep -q ":$PORT\b"; then
        error "Port $PORT/udp is already taken. Close the other copy or use -p."
    fi

    SERVER_ARGS="$GAME_ARGS /server"

    # The second window can wear ANOTHER theme, and that is not a gimmick: it
    # is the clearest way to see that the theme is decoration. Both windows
    # show the same fight, frame for frame, in two different paint jobs, and
    # neither desyncs because the walls come from the same place.
    CLIENT_ARGS="$GAME_ARGS /client"
    if [ -n "$THEME_CLIENT" ] && [ "$THEME_CLIENT" != "$THEME" ]; then
        CLIENT_ARGS="$(echo "$GAME_ARGS" | sed "s/ -$THEME/ -$THEME_CLIENT/")"
    fi

    # ------------------------------------------------------------
    # Only window 1 gets the intro.
    #
    # The game would sort it out on its own if both got it: the two ends
    # agree who plays it and the other one waits. But on ONE machine that is
    # silly, because you would be watching the same intro in one window and
    # a "waiting" message in the other. So window 2 is simply not given it.
    #
    # The one that keeps -demo is window 1, which is also the one that
    # starts the tunnel, so it matches what you get with two real machines
    # when you put -demo on the server.
    # ------------------------------------------------------------
    if [ "$DEMO" = "1" ]; then
        CLIENT_ARGS="$(echo "$CLIENT_ARGS" | sed 's/ -demo//')"
    fi

    prepare_run_dir "runserv"
    SERVER_RUN="$RUN_NAME"; SERVER_LOG="$LOG_FILE"
    SERVER_CONF="$CONF_DIR/generated-both-server.conf"
    generate_conf "$SERVER_CONF" "ipxnet startserver $PORT" "$SERVER_RUN" "$SERVER_ARGS"

    prepare_run_dir "runcli"
    CLIENT_RUN="$RUN_NAME"; CLIENT_LOG="$LOG_FILE"
    CLIENT_CONF="$CONF_DIR/generated-both-client.conf"
    generate_conf "$CLIENT_CONF" "ipxnet connect 127.0.0.1 $PORT" "$CLIENT_RUN" "$CLIENT_ARGS"

    summary
    grey "  window 1   : game.exe $SERVER_ARGS"
    grey "  window 2   : game.exe $CLIENT_ARGS"
    grey "--------------------------------------------------------"

    # If one window closes, the other goes with it instead of hanging around
    # holding the port.
    SERVER_PID=""; CLIENT_PID=""
    cleanup() {
        [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null
        [ -n "$CLIENT_PID" ] && kill "$CLIENT_PID" 2>/dev/null
        return 0
    }
    trap cleanup EXIT INT TERM

    green ""
    green "  Opening window 1 ..."

    # DOSBox is SDL 1 and reads the window position from the environment.
    # Without this the second one opens right on top of the first.
    SDL_VIDEO_WINDOW_POS="40,80" dosbox -conf "$SERVER_CONF" >/dev/null 2>&1 &
    SERVER_PID=$!

    # The tunnel has to be listening before the other one tries, or it gives
    # up with "Timeout connecting to server".
    sleep 3

    kill -0 "$SERVER_PID" 2>/dev/null || error "Window 1 died on startup."

    green "  Opening window 2 ..."
    green ""

    SDL_VIDEO_WINDOW_POS="720,80" dosbox -conf "$CLIENT_CONF" >/dev/null 2>&1 &
    CLIENT_PID=$!

    grey "  Click a window to give it the keyboard. Each one drives its own"
    grey "  tank: arrows to move, 5 on the numeric keypad to fire."
    grey ""
    grey "  Quit with ESC in BOTH of them."
    grey ""
    grey "  The logs, when you are done:"
    grey "      cat $SERVER_LOG"
    grey "      cat $CLIENT_LOG"
    grey ""

    wait "$SERVER_PID" 2>/dev/null
    wait "$CLIENT_PID" 2>/dev/null
    trap - EXIT INT TERM

    green ""
    green "  Both windows closed."
    green ""
    for pair in "WINDOW 1:$SERVER_LOG" "WINDOW 2:$CLIENT_LOG"; do
        role="${pair%%:*}"; file="${pair#*:}"
        if [ -f "$file" ]; then
            grey "  --- $role ---"
            grep -E "Memory at start|mem: near|could not load|Sound:|DESYNC|level" "$file" 2>/dev/null | sed 's/^/      /'
        else
            red "  $role wrote no log in $file"
        fi
    done
    grey ""

    exit 0

fi


# ============================================================
#  SERVER / CLIENT - two real machines
# ============================================================
if [ "$MODE" = "server" ]; then

    if command -v ss >/dev/null 2>&1 && ss -lun 2>/dev/null | grep -q ":$PORT\b"; then
        error "Port $PORT/udp is already taken. Close the other copy or use -p."
    fi

    prepare_run_dir "runserv"
    CONF="$CONF_DIR/generated-server.conf"
    generate_conf "$CONF" "ipxnet startserver $PORT" "$RUN_NAME" "$GAME_ARGS"

    summary

    # The command to hand to the other machine, translated back from the
    # game's own flags into play.sh's.
    #
    # -demo is deliberately NOT passed on. The two ends agree who shows the
    # intro, and with it on both of them the tie is broken by player 1, which
    # is a coin toss between two random ids: you would get the intro on this
    # machine some nights and on the other one the rest. Leaving it here only
    # means the client waits, which is the point.
    OTHER="$(echo "$GAME_ARGS" | sed 's|/net||; s|-demo||; s|/bigmap|-b|; s|-sky|-t sky|; s|-war|-t war|; s|-neon|-t neon|; s|-level\([1-5]\)|-l \1|')"

    green ""
    green "  SERVER ready. On the OTHER machine:"
    green ""
    for ip in $(ip -4 -o addr show scope global 2>/dev/null | awk '{split($4,a,"/"); print a[1]}'); do
        green "      ./play.sh client $ip -p $PORT$OTHER"
    done
    green ""
    grey  "  Give it the SAME -b and -l. The -t can differ: it is only"
    grey  "  paint, the three themes share the walls."

    if [ "$DEMO" = "1" ]; then
        grey  ""
        grey  "  The intro plays HERE. The other machine waits for it and"
        grey  "  says so on screen. Do not give it -demo as well, and do not"
        grey  "  worry about starting it late: it will wait."
    fi
    grey  ""

    if command -v ufw >/dev/null 2>&1 && ! ufw status 2>/dev/null | grep -q "$PORT/udp"; then
        grey "  If the other one hangs on 'Timeout connecting to server', open"
        grey "  the firewall on THIS machine:   sudo ufw allow $PORT/udp"
        grey ""
    fi

    grey "  Quit with ESC, not by closing the window."
    grey ""

    exec dosbox -conf "$CONF"

fi


if [ "$MODE" = "client" ]; then

    # If it does not even answer a ping, the problem is the network and not the
    # game. Worth knowing BEFORE going into graphics mode, where nothing can be
    # read any more.
    if command -v ping >/dev/null 2>&1 && ! ping -c 1 -W 2 "$SERVER_IP" >/dev/null 2>&1; then
        red  "WARNING: $SERVER_IP does not answer a ping."
        grey "  It is usually the cable, a mistyped IP, or the server's"
        grey "  firewall. If the ping does not work, the game will not either."
        grey ""
        printf "  Carry on anyway? [y/N] "
        read -r answer
        case "$answer" in y|Y|s|S) ;; *) exit 1 ;; esac
    fi

    prepare_run_dir "runcli"
    CONF="$CONF_DIR/generated-client.conf"
    generate_conf "$CONF" "ipxnet connect $SERVER_IP $PORT" "$RUN_NAME" "$GAME_ARGS"

    summary
    green ""
    green "  CLIENT connecting to $SERVER_IP:$PORT"
    green ""
    grey  "  If you get 'Timeout connecting to server':"
    grey  "    - the server is not running yet, or"
    grey  "    - its firewall is blocking the port:  sudo ufw allow $PORT/udp"
    grey  ""
    grey  "  Quit with ESC, not by closing the window."
    grey  ""

    exec dosbox -conf "$CONF"

fi
