#!/usr/bin/env bash
# Runs the linux-port Skyrim Together dedicated server under Proton.
#
# The server ships inside the mod build (SkyrimTogetherServer.exe + STServer.dll),
# so there is nothing to compile -- it is a Windows binary run through Proton,
# exactly like the client.
#
#   ./run_server.sh           start at 60 Hz (premium)
#   ./run_server.sh 30        start at 30 Hz
#   ./run_server.sh stop      stop the server
#   ./run_server.sh logs      follow the server log
#
# Halcyon Context prototype (RFC-0001), off unless asked for:
#   ./run_server.sh contexts on    enable, then start
#   ./run_server.sh contexts off   disable, then start
#   ./run_server.sh contexts       show the current setting
#
# WARNING: the Context prototype is unvalidated. Enabling it changes how
# quest-critical actor death is replicated and persists scoped state to
# config/halcyon-contexts.txt. See docs/RFC/0001-context-system.md.
#
# Console commands (TogglePremium, version, ShowMoPoStatus) are typed into the
# server's stdin; see ./run_server.sh cmd <command>.

set -uo pipefail

GAMEDIR="$HOME/.local/share/Steam/steamapps/common/Skyrim Special Edition"
DST="$GAMEDIR/Data/SkyrimTogetherReborn"

# Native Linux server, as produced by the halcyon-linux-server workflow. It is
# a thin runner next to libSTServer.so; both must sit in the same directory.
#
# Resolved to an absolute path: the launch path cds into the run directory, and
# a relative NATIVE_DIR would stop resolving from there.
NATIVE_DIR="${HALCYON_NATIVE_DIR:-$(cd "$(dirname "$0")" && pwd)/Halcyon Linux Server}"

# IMPORTANT: use the same Proton the game is launched with, and a SEPARATE prefix.
# Running this in the game's prefix (compatdata/489830) rewrites its `version`
# file and leaves the game hanging on the logo/menu with an orphaned wineserver
# ("wine client error: version mismatch"). Keep the server fully isolated.
PROTON="$HOME/.local/share/Steam/compatibilitytools.d/GE-Proton11-1/proton"
SERVER_PREFIX="$HOME/STR/server-prefix"
PIDFILE="/tmp/st-server.pid"
OUTLOG="/tmp/st-server-console.log"

# Prefer the native Linux build when it is present: it needs no Proton prefix
# and its console reads from the terminal directly. Set HALCYON_BACKEND=proton
# to force the Windows binary instead.
if [ "${HALCYON_BACKEND:-auto}" = proton ]; then
    BACKEND=proton
elif [ -x "$NATIVE_DIR/SkyrimTogetherServer" ]; then
    BACKEND=native
else
    BACKEND=proton
fi

if [ "$BACKEND" = native ]; then
    RUNDIR="$NATIVE_DIR"
    # The server is launched after a cd into RUNDIR, so it shows up in the
    # process table as "./SkyrimTogetherServer". Matching the full path here
    # would never hit, making status report a running server as stopped and
    # leaving stop unable to kill it.
    PROCPAT='\./SkyrimTogetherServer$'
else
    RUNDIR="$DST"
    PROCPAT=SkyrimTogetherServer.exe
fi

INI="$RUNDIR/config/STServer.ini"

# Sets key=value inside [section], creating either if absent. The server
# rewrites this file on shutdown, so only call this while it is stopped.
ini_set() {
    local section="$1" key="$2" value="$3"

    mkdir -p "$(dirname "$INI")"
    [ -f "$INI" ] || : >"$INI"

    if ! grep -q "^\[$section\]" "$INI"; then
        printf '\n[%s]\n%s=%s\n' "$section" "$key" "$value" >>"$INI"
        return
    fi

    # Rewrite the key only within its own section, so an identically named key
    # under another section is left alone.
    awk -v section="[$section]" -v key="$key" -v value="$value" '
        /^\[/ { insection = ($0 == section) }
        insection && $0 ~ "^" key "=" { print key "=" value; found = 1; next }
        { print }
        END { if (!found) exit 1 }
    ' "$INI" >"$INI.tmp" && mv "$INI.tmp" "$INI" && return

    rm -f "$INI.tmp"

    # Key absent: append it directly under its section header.
    awk -v section="[$section]" -v key="$key" -v value="$value" '
        { print }
        $0 == section { print key "=" value }
    ' "$INI" >"$INI.tmp" && mv "$INI.tmp" "$INI"
}

ini_get() {
    local section="$1" key="$2"

    [ -f "$INI" ] || return 1

    awk -v section="[$section]" -v key="$key" '
        /^\[/ { insection = ($0 == section) }
        insection && $0 ~ "^" key "=" { sub("^" key "=", ""); print; exit }
    ' "$INI"
}

case "${1:-start}" in
contexts)
    case "${2:-show}" in
    on | off)
        if pgrep -f SkyrimTogetherServer >/dev/null; then
            echo "stop the server first: the server rewrites STServer.ini on shutdown" >&2
            exit 1
        fi
        [ "${2}" = on ] && want=true || want=false
        ini_set Halcyon bEnableContexts "$want"
        echo "set Halcyon:bEnableContexts=$want"
        if [ "$want" = true ]; then
            cat <<'WARN'

WARNING: the Context prototype is unvalidated (RFC-0001).
Quest-critical actor death becomes scoped per player, and scoped state is
written to config/halcyon-contexts.txt. Players are identified by username,
which the server does not verify -- two players using one name share state.
WARN
        fi
        exit 0
        ;;
    show)
        current="$(ini_get Halcyon bEnableContexts)"
        echo "backend: $BACKEND ($RUNDIR)"
        echo "Halcyon:bEnableContexts=${current:-<unset, defaults to false>}"
        if [ -z "$current" ] && [ -f "$INI" ]; then
            echo "  note: the key is absent, so this server build may predate it;"
            echo "  it is written by the server itself once a build that has it runs."
        fi
        store="$RUNDIR/config/halcyon-contexts.txt"
        if [ -f "$store" ]; then
            echo "context store: $store ($(wc -l <"$store") lines)"
        else
            echo "context store: none yet"
        fi
        exit 0
        ;;
    *)
        echo "usage: $0 contexts [on|off]" >&2
        exit 2
        ;;
    esac
    ;;
stop)
    if [ -f "$PIDFILE" ]; then
        kill "$(cat "$PIDFILE")" 2>/dev/null
        rm -f "$PIDFILE"
    fi
    pkill -f "$PROCPAT" 2>/dev/null
    echo "server stopped ($BACKEND)"
    exit 0
    ;;
logs)
    # The server writes spdlog output under logs/ next to the binary.
    tail -f "$RUNDIR/logs/"*erver*.log 2>/dev/null || tail -f "$OUTLOG"
    exit 0
    ;;
status)
    if pgrep -f "$PROCPAT" >/dev/null; then
        echo "server RUNNING ($BACKEND, pid $(pgrep -f "$PROCPAT" | head -1))"
        ss -ulpn 2>/dev/null | grep -E ':10578' || echo "  (udp/10578 not bound yet)"
    else
        echo "server NOT running ($BACKEND backend selected)"
    fi
    exit 0
    ;;
esac

PREMIUM=true
case "${1:-}" in
30) PREMIUM=false ;;
60 | start | "") PREMIUM=true ;;
*) echo "usage: $0 [30|60|stop|logs|status|contexts]" >&2; exit 2 ;;
esac

if [ "$BACKEND" = native ]; then
    [ -x "$NATIVE_DIR/SkyrimTogetherServer" ] || { echo "native server not found in $NATIVE_DIR" >&2; exit 1; }
    # RUNPATH is $ORIGIN, so the runner only finds its library beside itself.
    [ -f "$NATIVE_DIR/libSTServer.so" ] || {
        echo "libSTServer.so missing from $NATIVE_DIR" >&2
        echo "the runner cannot start without it; re-download the full artifact" >&2
        exit 1
    }
else
    [ -x "$PROTON" ] || { echo "Proton not found at $PROTON" >&2; exit 1; }
    [ -f "$DST/SkyrimTogetherServer.exe" ] || { echo "server exe not found in $DST" >&2; exit 1; }
fi

# Seed the config so the tick rate is set before first launch. The server
# creates config/STServer.ini itself on first run if this is absent.
pkill -f "$PROCPAT" 2>/dev/null
sleep 1

# The server rewrites config/STServer.ini on shutdown (it registers its settings
# after load), so edit it only while the server is stopped -- which is here.
mkdir -p "$RUNDIR/config"
if [ -f "$INI" ]; then
    ini_set GameServer bPremiumMode "$PREMIUM"
    echo "set bPremiumMode=$PREMIUM in STServer.ini"
else
    # First run: the server generates the file itself with sane defaults
    # (bPremiumMode=true, bEnableModCheck=false), so nothing to seed.
    echo "no STServer.ini yet - the server will generate it (defaults to 60 Hz)"
fi

# Capture the setting while the file is still stable, before the server starts
# rewriting it.
WANT_CONTEXTS="$(ini_get Halcyon bEnableContexts)"

: >"$OUTLOG"
# NOTE: the server console uses uv_tty and only reads from a real terminal, so
# piping stdin does not work. Change settings through config/STServer.ini and
# restart instead (this script's 30/60 and contexts arguments do exactly that).
if [ "$BACKEND" = native ]; then
    cd "$NATIVE_DIR" || exit 1
    setsid ./SkyrimTogetherServer </dev/null >>"$OUTLOG" 2>&1 &
else
    mkdir -p "$SERVER_PREFIX"
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="$HOME/.local/share/Steam"
    export STEAM_COMPAT_DATA_PATH="$SERVER_PREFIX"
    # A non-Skyrim app id keeps this prefix unrelated to the game's own.
    export STEAM_COMPAT_APP_ID=0
    export SteamAppId=0

    cd "$DST" || exit 1
    WINPATH="Z:$(pwd | tr '/' '\\')"

    setsid "$PROTON" run "$WINPATH\\SkyrimTogetherServer.exe" </dev/null >>"$OUTLOG" 2>&1 &
fi
echo $! >"$PIDFILE"

echo "starting $BACKEND server (premium=$PREMIUM -> $([ "$PREMIUM" = true ] && echo 60 || echo 30) Hz)..."
# The native server comes up immediately; Proton needs time to build its prefix.
[ "$BACKEND" = native ] && sleep 3 || sleep 12
tail -25 "$OUTLOG"

# Read from the value staged before launch: the server rewrites STServer.ini
# itself, so reading it back here can catch the file mid-rewrite.
case "$WANT_CONTEXTS" in
true) CONTEXTS_STATE="ENABLED (unvalidated prototype, RFC-0001)" ;;
false) CONTEXTS_STATE="disabled" ;;
*) CONTEXTS_STATE="not set (server defaults it to disabled)" ;;
esac

LANIP=$(ip -4 addr show scope global 2>/dev/null | awk '/inet /{sub(/\/.*/,"",$2); print $2; exit}')
cat <<EOF

--------------------------------------------------------------------
Connect from the client overlay (F2):
    Address   127.0.0.1     (same PC)   or   ${LANIP:-<no LAN ip>}
    Port      10578
    Password  <empty>

Tick rate:               $0 30   /   $0 60   (restarts the server)
Context prototype:       $0 contexts on|off   (server must be stopped)
Follow log:              $0 logs
Status:                  $0 status
Stop:                    $0 stop

Contexts: ${CONTEXTS_STATE}
--------------------------------------------------------------------
EOF
