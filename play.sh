#!/usr/bin/env bash
# ============================================================
# CutreGame - el unico script para jugar.
#
#   ./play.sh                              la ayuda
#   ./play.sh local                        dos jugadores, un teclado
#   ./play.sh both                         dos ventanas aqui, mapa pequeno
#   ./play.sh both   -b -t neon -l 3       dos ventanas aqui, mapa grande
#   ./play.sh server -b -t war  -l 3       ordenador 1 de 2
#   ./play.sh client <ip> -b -t war -l 3   ordenador 2 de 2
#
# Antes esto eran cinco ficheros. Es uno porque los cuatro modos comparten
# casi todo: las mismas comprobaciones, el mismo .conf, el mismo directorio
# de trabajo. Lo unico que cambia es la linea de ipxnet y cuantas ventanas
# se abren.
#
# NADA de aqui lleva una ruta escrita a mano: todo sale de donde esta este
# fichero, asi que funciona en cualquier maquina y desde cualquier carpeta.
# Una ruta metida a mano en un .conf es justo lo que se rompe al llevarlo a
# la segunda maquina.
# ============================================================
set -u

GAME_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PORT=5213
CYCLES="fixed 30000"
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

  CutreGame - como jugar

  UN ORDENADOR
    ./play.sh local                      dos jugadores en un teclado
    ./play.sh both                       dos ventanas, mapa pequeno
    ./play.sh both -b                    dos ventanas, mapa grande
    ./play.sh both -b -t neon -l 3       ... con tema y nivel
    ./play.sh local -demo                con la intro delante

  DOS ORDENADORES
    ./play.sh server -b -t war -l 3           en el primero
    ./play.sh client 192.168.1.45 -b -t war -l 3   en el segundo

    Arranca SIEMPRE el server primero. El te dice la IP que hay que
    escribir en el otro.

  OPCIONES
    -b            mapa grande 640x400 con camara (sin esto, 320x200)
    -t TEMA       sky, war o neon.  Necesita -b
    -l N          nivel 1 a 5.      Necesita -b
    -T TEMA       solo en 'both': tema de la SEGUNDA ventana
    -d, -demo     la intro: 15 imagenes con efectos antes de jugar
    -p PUERTO     puerto UDP del tunel. Por defecto 5213
    -y CICLOS     cycles= de DOSBox. Por defecto "fixed 30000"

  QUE TIENE QUE COINCIDIR ENTRE LAS DOS MAQUINAS
    -b   SI.  Mapas de distinto tamano = partidas distintas.
    -l   SI.  Cada nivel tiene muros distintos.
    -t   NO.  Es solo pintura: los tres temas comparten los muros.

    Si los -l no coinciden el juego lo negocia en vez de romperse, pero
    gana el JUGADOR 1, que se sortea al azar en cada partida y NO es
    quien lanzo el server. Asi que ponlo igual en las dos.

  LA INTRO  (-d)
    Ensena res/demo/demo01.bmp .. demo15.bmp, cada una con un efecto
    distinto, 8 segundos, con la musica. ESC se la salta y empieza la
    partida.

    Las imagenes tienen que ser BMP de 320x200 y 256 COLORES, de 65078
    bytes. Una que falte se salta sin mas.

    La intro va ANTES de emparejar por red, asi que en 'both', 'server'
    y 'client' cada maquina ve la suya y luego se buscan. Si en una le
    das a ESC y en la otra no, la primera espera: no pasa nada.

  CONTROLES
    local     jugador 1: flechas + 5 del numerico
              jugador 2: W A S D  + G
    en red    los dos con flechas + 5 del numerico
    ESC para salir (no cierres la ventana: el log no se termina de escribir)

END
exit "${1:-0}"
}


# ------------------------------------------------------------
# El modo es la primera palabra, y la IP la segunda si el modo es client.
# ------------------------------------------------------------
MODE="${1:-}"
[ -z "$MODE" ] && usage 0
shift

case "$MODE" in
    local|both|server|client) ;;
    -h|--help|help)           usage 0 ;;
    *) red "Modo desconocido: '$MODE'"; usage 1 ;;
esac

if [ "$MODE" = "client" ]; then
    if [ $# -gt 0 ] && [ "${1:0:1}" != "-" ]; then
        SERVER_IP="$1"
        shift
    fi
fi

MODE_NAME="$MODE"
GAME_ARGS=""
[ "$MODE" != "local" ] && GAME_ARGS="/net"

# ------------------------------------------------------------
# getopts solo entiende opciones de UNA letra: le pasas -demo y lo lee como
# -d -e -m -o. Como el juego se llama con "-demo" y es lo que uno escribe
# sin pensar, se saca de la lista antes y se acepta de las dos formas.
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

while getopts "bdt:T:l:p:y:h" option; do
    case "$option" in
        b) GAME_ARGS="$GAME_ARGS /bigmap" ;;
        d) DEMO=1 ;;
        t) THEME="$OPTARG" ;;
        T) THEME_CLIENT="$OPTARG" ;;
        l) LEVEL="$OPTARG" ;;
        p) PORT="$OPTARG" ;;
        y) CYCLES="$OPTARG" ;;
        h) usage 0 ;;
        *) usage 1 ;;
    esac
done

BIGMAP=0
[ "$GAME_ARGS" != "${GAME_ARGS#*bigmap}" ] && BIGMAP=1


# ------------------------------------------------------------
# El tema solo viste el mapa grande, y el nivel solo existe ahi.
#
# El tema es decoracion pura: los tres comparten mapa de colisiones byte a
# byte, asi que las dos maquinas pueden llevar temas distintos. El nivel no:
# cada uno tiene sus propios muros.
# ------------------------------------------------------------
if [ -n "$THEME" ]; then

    case "$THEME" in
        sky|war|neon) ;;
        *) error "Tema '$THEME' desconocido. Tiene que ser sky, war o neon." ;;
    esac

    [ "$BIGMAP" = "0" ] && error "-t necesita -b: los temas solo visten el mapa grande."

    GAME_ARGS="$GAME_ARGS -$THEME"

fi

if [ -n "$LEVEL" ]; then

    case "$LEVEL" in
        1|2|3|4|5) ;;
        *) error "Nivel '$LEVEL' desconocido. Tiene que ser de 1 a 5." ;;
    esac

    [ "$BIGMAP" = "0" ] && error "-l necesita -b: los niveles solo existen en el mapa grande."

    # Los niveles solo vienen vestidos, no hay res/15Level/ORIGINAL. El juego
    # hace lo mismo por su cuenta; hacerlo aqui tambien es lo que permite
    # comprobar mas abajo que los ficheros estan.
    if [ -z "$THEME" ]; then
        THEME="sky"
        GAME_ARGS="$GAME_ARGS -sky"
        grey "  (los niveles solo vienen vestidos: el $LEVEL usa -sky)"
    fi

    GAME_ARGS="$GAME_ARGS -level$LEVEL"

fi

if [ -n "$THEME_CLIENT" ]; then
    [ "$MODE" != "both" ] && error "-T solo tiene sentido en 'both'."
    case "$THEME_CLIENT" in
        sky|war|neon) ;;
        *) error "Tema '$THEME_CLIENT' desconocido." ;;
    esac
fi

[ "$MODE" = "client" ] && [ -z "$SERVER_IP" ] && \
    { red "Falta la IP del servidor."; usage 1; }

[ "$MODE" != "local" ] && [ "$PORT" -lt 1024 ] && \
    error "El puerto $PORT es privilegiado: en Linux solo root puede abrirlo. Usa uno por encima de 1024."


# ------------------------------------------------------------
# Todo lo que hace falta, comprobado antes de abrir nada. Cada fallo dice
# exactamente que pasa y como arreglarlo: eso es lo que convierte un "no
# funciona" en una linea concreta.
# ------------------------------------------------------------
command -v dosbox >/dev/null 2>&1 || error \
"DOSBox no esta instalado.
       Instalalo con:  sudo apt install dosbox"

EXE="$(find "$GAME_ROOT/bin" -maxdepth 1 -iname 'game.exe' 2>/dev/null | head -1)"
[ -n "$EXE" ] || error \
"No encuentro bin/GAME.EXE
       Compila primero y copialo a $GAME_ROOT/bin/"

[ -d "$GAME_ROOT/res" ] || error \
"No encuentro la carpeta res/ dentro de $GAME_ROOT
       El juego busca sus recursos en ..\\res\\ y no arranca sin ella."

if [ "$BIGMAP" = "1" ]; then

    case "$THEME" in
        sky)  spr=spr_sky.bmp;  folder=SKYNET  ;;
        war)  spr=spr_war.bmp;  folder=MILITAR ;;
        neon) spr=spr_neon.bmp; folder=NEON    ;;
        *)    spr=sprites.bmp;  folder=SKYNET  ;;
    esac

    [ -f "$GAME_ROOT/res/$spr" ] || error "Falta res/$spr (la hoja de sprites del tema)."

    if [ -z "$LEVEL" ]; then

        case "$THEME" in
            sky)  map=map_sky.bmp  ;;
            war)  map=map_war.bmp  ;;
            neon) map=map_neon.bmp ;;
            *)    map=big.bmp      ;;
        esac

        [ -f "$GAME_ROOT/res/$map" ]        || error "Falta res/$map"
        [ -f "$GAME_ROOT/res/bigcol.bmp" ]  || error "Falta res/bigcol.bmp (el mapa de colisiones)."

    else

        # Cada nivel trae SU PROPIO bigcol.bmp, y por eso hay que ponerse de
        # acuerdo por la red: un nivel distinto son muros distintos.
        dir="$GAME_ROOT/res/15Level/$folder/NIVEL0$LEVEL"

        [ -d "$dir" ] || error \
"No encuentro $dir
       Los niveles 1 a 5 viven en res/15Level/<TEMA>/NIVELnn/ y cada uno
       trae su big.bmp y su bigcol.bmp."

        for f in big.bmp bigcol.bmp; do
            [ -f "$dir/$f" ] || error "Al nivel $LEVEL del tema '$THEME' le falta $f
       Lo busque en $dir"
        done

    fi

fi


# ------------------------------------------------------------
# LA INTRO
#
# Va la ultima de la linea porque asi se lee bien lo que se ejecuta, y al
# juego el orden de los argumentos le da igual.
#
# Las imagenes se comprueban aqui por lo mismo que los niveles: el juego se
# salta en silencio la que no encuentre, asi que sin esto una intro con las
# quince mal puestas seria una pantalla en negro de dos minutos sin que
# nadie te diga por que.
# ------------------------------------------------------------
if [ "$DEMO" = "1" ]; then

    GAME_ARGS="$GAME_ARGS -demo"

    [ -d "$GAME_ROOT/res/demo" ] || error \
"No encuentro la carpeta res/demo/
       La intro busca ahi res/demo/demo01.bmp .. demo15.bmp"

    DEMO_FOUND=0
    DEMO_BAD=""

    for n in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15; do

        img="$GAME_ROOT/res/demo/demo$n.bmp"
        [ -f "$img" ] || continue

        DEMO_FOUND=$((DEMO_FOUND + 1))

        # 65078 bytes exactos: 54 de cabecera + 1024 de paleta + 320*200.
        # Un BMP de 256 colores que use menos colores sale mas corto, y el
        # juego hace fseek(1078) a pelo: la imagen saldria descuadrada en
        # diagonal en vez de dar un error.
        size=$(stat -c %s "$img")
        [ "$size" = "65078" ] || DEMO_BAD="$DEMO_BAD demo$n.bmp($size)"

    done

    [ "$DEMO_FOUND" -gt 0 ] || error \
"En res/demo/ no hay ninguna imagen.
       Tienen que llamarse demo01.bmp .. demo15.bmp, ser BMP de 320x200 y
       256 colores, y ocupar 65078 bytes exactos."

    if [ -n "$DEMO_BAD" ]; then
        red "AVISO: estas no miden 65078 bytes y saldran torcidas:"
        red "      $DEMO_BAD"
        grey "  Guardalas como BMP de 320x200 con la paleta de 256 entradas completa."
    fi

fi


# ------------------------------------------------------------
# Cada instancia corre desde SU PROPIO directorio, y no es por orden.
#
# El juego escribe su log con fopen("game.log"), una ruta RELATIVA, asi que
# cae en el directorio donde este DOS. Si las dos instancias corrieran desde
# el mismo sitio se pisarian el log y el de una simplemente no existiria.
#
# Tiene que colgar de la raiz del proyecto, porque el juego pide sus recursos
# como ..\res\ y eso tiene que dar con la res/ de verdad.
#
# Y el nombre tiene que caber en el 8.3 de DOS: 8 caracteres, sin punto. Con
# mas, DOS no lo rechaza, lo DESTROZA (runserver -> RUNSER~1) y el cd del
# autoexec deja de encontrarlo.
# ------------------------------------------------------------
prepare_run_dir() {

    local name="$1"

    [ ${#name} -gt 8 ] && error "El directorio '$name' no cabe en el 8.3 de DOS."

    RUN_DIR="$GAME_ROOT/$name"
    mkdir -p "$RUN_DIR"
    cp -f "$EXE" "$RUN_DIR/GAME.EXE"
    rm -f "$RUN_DIR/GAME.LOG" "$RUN_DIR/game.log"

    RUN_NAME="$name"
    LOG_FILE="$RUN_DIR/GAME.LOG"

}


# ------------------------------------------------------------
# Escribe el .conf de DOSBox con las rutas ya resueltas.
#   $1 fichero destino   $2 linea de ipxnet (vacia = sin red)
#   $3 directorio de trabajo   $4 argumentos del juego
# ------------------------------------------------------------
generate_conf() {

    local target="$1" ipx_line="$2" run_name="$3" args="$4"
    local ipx_enabled="true"

    [ -z "$ipx_line" ] && ipx_enabled="false"

    mkdir -p "$(dirname "$target")"

    cat > "$target" <<CONF_EOF
# Generado por play.sh el $(date '+%Y-%m-%d %H:%M'). No lo edites: se reescribe.
# Modo: $MODE_NAME    linea: game.exe $args

[dosbox]
machine=svga_s3

[cpu]
core=auto
# Las dos maquinas tienen que ir a los mismos ciclos. El lockstep sincroniza
# por frame y no por tiempo, asi que no vas a desincronizar por ir a
# velocidades distintas, pero la rapida se pasaria media vida esperando.
cycles=$CYCLES

[sblaster]
sbtype=sb16
sbbase=220
irq=7
dma=1
hdma=5

[ipx]
# DOSBox no emula una tarjeta de red: emula IPX directamente. Nada de LSL,
# ODI, IPXODI, NET.CFG ni frame types. No hay que instalar nada.
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
    grey "  modo       : $MODE_NAME"
    [ "$BIGMAP" = "1" ] && grey "  tema       : ${THEME:-original}"
    [ "$BIGMAP" = "1" ] && grey "  nivel      : ${LEVEL:-0}   (0 = el mapa grande original)"
    [ "$DEMO" = "1" ]   && grey "  intro      : si, $DEMO_FOUND imagenes de res/demo/  (ESC se la salta)"
    grey "  linea      : game.exe $GAME_ARGS"
    grey "  ejecutable : $(date -r "$EXE" '+%Y-%m-%d %H:%M')"
    [ "$MODE" != "local" ] && grey "  puerto     : $PORT/udp"
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
    green "  Dos jugadores en un teclado."
    green ""
    grey  "    jugador 1   flechas, disparo con el 5 del numerico"
    grey  "    jugador 2   W A S D,  disparo con G"
    grey  ""
    grey  "  Sal con ESC, no cerrando la ventana."
    grey  ""

    exec dosbox -conf "$CONF"

fi


# ============================================================
#  BOTH - las dos ventanas en esta maquina
# ============================================================
if [ "$MODE" = "both" ]; then

    if command -v ss >/dev/null 2>&1 && ss -lun 2>/dev/null | grep -q ":$PORT\b"; then
        error "El puerto $PORT/udp ya esta ocupado. Cierra la otra copia o usa -p."
    fi

    SERVER_ARGS="$GAME_ARGS"

    # La segunda ventana puede llevar OTRO tema, y no es un capricho: es la
    # forma mas clara de ver que el tema es decoracion. Las dos ventanas
    # ensenan el mismo combate, frame a frame, con dos pinturas distintas, y
    # ninguna desincroniza porque los muros salen del mismo sitio.
    CLIENT_ARGS="$GAME_ARGS"
    if [ -n "$THEME_CLIENT" ] && [ "$THEME_CLIENT" != "$THEME" ]; then
        CLIENT_ARGS="$(echo "$GAME_ARGS" | sed "s/ -$THEME/ -$THEME_CLIENT/")"
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
    grey "  ventana 1  : game.exe $SERVER_ARGS"
    grey "  ventana 2  : game.exe $CLIENT_ARGS"
    grey "--------------------------------------------------------"

    # Si una ventana se cierra, la otra se va con ella en vez de quedarse
    # colgada ocupando el puerto.
    SERVER_PID=""; CLIENT_PID=""
    cleanup() {
        [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null
        [ -n "$CLIENT_PID" ] && kill "$CLIENT_PID" 2>/dev/null
        return 0
    }
    trap cleanup EXIT INT TERM

    green ""
    green "  Abriendo la ventana 1 ..."

    # DOSBox es SDL 1 y lee la posicion de la ventana del entorno. Sin esto
    # la segunda se abre justo encima de la primera.
    SDL_VIDEO_WINDOW_POS="40,80" dosbox -conf "$SERVER_CONF" >/dev/null 2>&1 &
    SERVER_PID=$!

    # El tunel tiene que estar escuchando antes de que el otro lo intente, o
    # se rinde con "Timeout connecting to server".
    sleep 3

    kill -0 "$SERVER_PID" 2>/dev/null || error "La ventana 1 se murio al arrancar."

    green "  Abriendo la ventana 2 ..."
    green ""

    SDL_VIDEO_WINDOW_POS="720,80" dosbox -conf "$CLIENT_CONF" >/dev/null 2>&1 &
    CLIENT_PID=$!

    grey "  Haz clic en una ventana para darle el teclado. Cada una lleva su"
    grey "  tanque: flechas para moverse, 5 del numerico para disparar."
    grey ""
    grey "  Sal con ESC en LAS DOS."
    grey ""
    grey "  Los logs, al terminar:"
    grey "      cat $SERVER_LOG"
    grey "      cat $CLIENT_LOG"
    grey ""

    wait "$SERVER_PID" 2>/dev/null
    wait "$CLIENT_PID" 2>/dev/null
    trap - EXIT INT TERM

    green ""
    green "  Las dos ventanas cerradas."
    green ""
    for pair in "VENTANA 1:$SERVER_LOG" "VENTANA 2:$CLIENT_LOG"; do
        role="${pair%%:*}"; file="${pair#*:}"
        if [ -f "$file" ]; then
            grey "  --- $role ---"
            grep -E "Memory at start|mem: near|could not load|Sound:|DESYNC|level" "$file" 2>/dev/null | sed 's/^/      /'
        else
            red "  $role no escribio log en $file"
        fi
    done
    grey ""

    exit 0

fi


# ============================================================
#  SERVER / CLIENT - dos maquinas de verdad
# ============================================================
if [ "$MODE" = "server" ]; then

    if command -v ss >/dev/null 2>&1 && ss -lun 2>/dev/null | grep -q ":$PORT\b"; then
        error "El puerto $PORT/udp ya esta ocupado. Cierra la otra copia o usa -p."
    fi

    prepare_run_dir "runserv"
    CONF="$CONF_DIR/generated-server.conf"
    generate_conf "$CONF" "ipxnet startserver $PORT" "$RUN_NAME" "$GAME_ARGS"

    summary

    OTHER="$(echo "$GAME_ARGS" | sed 's|/net||; s|/bigmap|-b|; s|-sky|-t sky|; s|-war|-t war|; s|-neon|-t neon|; s|-level\([1-5]\)|-l \1|')"

    green ""
    green "  SERVIDOR listo. En el OTRO ordenador:"
    green ""
    for ip in $(ip -4 -o addr show scope global 2>/dev/null | awk '{split($4,a,"/"); print a[1]}'); do
        green "      ./play.sh client $ip -p $PORT$OTHER"
    done
    green ""
    grey  "  Ponle los MISMOS -b y -l. El -t puede ser distinto: es solo"
    grey  "  pintura, los tres temas comparten los muros."
    grey  ""

    if command -v ufw >/dev/null 2>&1 && ! ufw status 2>/dev/null | grep -q "$PORT/udp"; then
        grey "  Si el otro se queda en 'Timeout connecting to server', abre el"
        grey "  cortafuegos de ESTA maquina:   sudo ufw allow $PORT/udp"
        grey ""
    fi

    grey "  Sal con ESC, no cerrando la ventana."
    grey ""

    exec dosbox -conf "$CONF"

fi


if [ "$MODE" = "client" ]; then

    # Si no responde ni al ping, el problema es la red y no el juego. Vale la
    # pena saberlo ANTES de pasar a modo grafico, donde ya no se lee nada.
    if command -v ping >/dev/null 2>&1 && ! ping -c 1 -W 2 "$SERVER_IP" >/dev/null 2>&1; then
        red  "AVISO: $SERVER_IP no responde al ping."
        grey "  Suele ser el cable, una IP mal escrita, o el cortafuegos del"
        grey "  servidor. Si el ping no va, el juego tampoco."
        grey ""
        printf "  Sigo de todas formas? [s/N] "
        read -r answer
        case "$answer" in s|S|y|Y) ;; *) exit 1 ;; esac
    fi

    prepare_run_dir "runcli"
    CONF="$CONF_DIR/generated-client.conf"
    generate_conf "$CONF" "ipxnet connect $SERVER_IP $PORT" "$RUN_NAME" "$GAME_ARGS"

    summary
    green ""
    green "  CLIENTE conectando a $SERVER_IP:$PORT"
    green ""
    grey  "  Si sale 'Timeout connecting to server':"
    grey  "    - el servidor no esta arrancado todavia, o"
    grey  "    - su cortafuegos bloquea el puerto:  sudo ufw allow $PORT/udp"
    grey  ""
    grey  "  Sal con ESC, no cerrando la ventana."
    grey  ""

    exec dosbox -conf "$CONF"

fi
