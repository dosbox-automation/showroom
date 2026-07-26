#!/bin/bash
# This file is part of the dosbox-automation-showroom Project.
# License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
#
# Lifecycle for the showroom drive instance of dosbox-automation.
# start [--headless] | stop | status
#
# Recordings land in .games-cache/recordings (capture_dir), installs
# in .games-cache/installs, downloaded media in .games-cache/downloads.
# The games cache must be listed in mount_allowed_bases in the PRIMARY
# config: the engine ignores that setting from -conf files by design.

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
CACHE_DIR="$REPO_ROOT/.games-cache"
DRIVE_DIR="$CACHE_DIR/.drive"
RECORDINGS_DIR="$CACHE_DIR/recordings"

DOSBOX="${DOSBOX:-$HOME/Projects/augrudottir/augrudottir-dosbox-automation/build/release-linux/dosbox}"
PORT="${SHOWROOM_DRIVE_PORT:-8386}"
API="http://localhost:$PORT/api/v1"
PRIMARY_CONF="$HOME/.config/dosbox-automation/dosbox-automation.conf"
TOKEN_FILE="$HOME/.config/dosbox-automation/webserver/api_token"
PID_FILE="$DRIVE_DIR/dosbox.pid"
RUN_CONF="$DRIVE_DIR/drive.conf"
LOG_FILE="$DRIVE_DIR/dosbox.log"

die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

token() {
    [[ -r "$TOKEN_FILE" ]] || die "no token file at $TOKEN_FILE (instance not up yet?)"
    cat "$TOKEN_FILE"
}

api_get() {
    curl -sf --max-time 5 -H "Authorization: Bearer $(token)" "$API/$1"
}

alive_pid() {
    [[ -f "$PID_FILE" ]] || return 1
    local pid
    pid="$(cat "$PID_FILE")"
    [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null && printf '%s' "$pid"
}

check_mount_policy() {
    [[ -r "$PRIMARY_CONF" ]] || die "primary config not found: $PRIMARY_CONF"
    if ! grep -E '^\s*mount_allowed_bases\s*=' "$PRIMARY_CONF" | grep -qF "$CACHE_DIR"; then
        die "games cache is not in mount_allowed_bases of the PRIMARY config.
Add it (semicolon-separated) to $PRIMARY_CONF:
  mount_allowed_bases = <existing paths>;$CACHE_DIR
The engine ignores this setting from -conf files by design."
    fi
}

cmd_start() {
    local headless=0
    [[ "${1:-}" == "--headless" ]] && headless=1

    [[ -x "$DOSBOX" ]] || die "dosbox binary not found or not executable: $DOSBOX"
    if pid="$(alive_pid)"; then
        die "already running (pid $pid). Use stop first."
    fi
    if curl -sf --max-time 2 -o /dev/null "http://localhost:$PORT/"; then
        die "port $PORT is already serving (another instance?). Refusing to start."
    fi
    check_mount_policy

    mkdir -p "$DRIVE_DIR" "$RECORDINGS_DIR" "$CACHE_DIR/installs" "$CACHE_DIR/downloads"

    cat > "$RUN_CONF" << EOF
[webserver]
webserver_enabled = true
webserver_port = $PORT
webserver_token_file = true

[cpu]
core = dynamic
cpu_cycles = 12000
cpu_cycles_protected = 12000

[capture]
capture_dir = $RECORDINGS_DIR
EOF

    local -a env_prefix=()
    (( headless )) && env_prefix=(env SDL_VIDEODRIVER=offscreen)

    "${env_prefix[@]}" "$DOSBOX" -conf "$RUN_CONF" > "$LOG_FILE" 2>&1 &
    local pid=$!
    printf '%s\n' "$pid" > "$PID_FILE"

    local _i
    for _i in $(seq 1 20); do
        sleep 0.5
        if [[ -r "$TOKEN_FILE" ]] && curl -sf --max-time 2 -o /dev/null \
                -H "Authorization: Bearer $(cat "$TOKEN_FILE")" "$API/status"; then
            printf 'up: pid %s, port %s, recordings -> %s\n' "$pid" "$PORT" "$RECORDINGS_DIR"
            return 0
        fi
        kill -0 "$pid" 2>/dev/null || { tail -5 "$LOG_FILE" >&2; die "dosbox exited during startup (log: $LOG_FILE)"; }
    done
    die "API did not come up within 10s (pid $pid still running, log: $LOG_FILE)"
}

cmd_stop() {
    local pid
    if ! pid="$(alive_pid)"; then
        printf 'not running\n'
        rm -f "$PID_FILE"
        return 0
    fi
    if curl -sf --max-time 5 -X POST -H "Authorization: Bearer $(token)" \
            "$API/dosbox/shutdown" -o /dev/null; then
        local _i
        for _i in $(seq 1 10); do
            kill -0 "$pid" 2>/dev/null || break
            sleep 0.5
        done
    fi
    if kill -0 "$pid" 2>/dev/null; then
        printf 'API shutdown did not finish, killing pid %s\n' "$pid"
        kill "$pid"
    fi
    rm -f "$PID_FILE"
    printf 'stopped\n'
}

cmd_status() {
    local pid
    if ! pid="$(alive_pid)"; then
        printf 'not running\n'
        return 1
    fi
    printf 'pid: %s\n' "$pid"
    printf 'status:    %s\n' "$(api_get status)"
    printf 'capture:   %s\n' "$(api_get capture/video/status)"
    printf 'recording: %s\n' "$(api_get input/record/status)"
}

case "${1:-}" in
    start)  shift; cmd_start "$@" ;;
    stop)   cmd_stop ;;
    status) cmd_status ;;
    *)      printf 'usage: %s start [--headless] | stop | status\n' "$0" >&2; exit 2 ;;
esac
