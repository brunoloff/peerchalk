#!/usr/bin/env bash
# Stops the PeerChalk TURN relay before its server's outgoing traffic costs money. Oracle
# Cloud includes 10 TB of outgoing traffic a month; past that it bills per GB. Run it every
# few minutes as root (setup-coturn.sh installs a systemd timer for it):
#
#   traffic-cutoff.sh [--stop-gb N] [--shutdown-gb N] [--interface NAME] [--check GB]
#
# It reads this calendar month's outgoing bytes from vnstat. At --stop-gb (default 5000,
# half the allowance) it stops coturn; at --shutdown-gb (default 8000) it powers the server
# off, in case something other than coturn keeps sending. A coturn it stopped starts again
# once a new month brings the count back under --stop-gb. A server it powered off stays off
# until started from the Oracle console.
#
# --check GB only prints what it would do with that many GB sent, changing nothing.
set -euo pipefail

stop_gb=5000
shutdown_gb=8000
interface=""
check_gb=""
# Set when it stopped coturn; PEERCHALK_CUTOFF_MARKER moves it, for tests.
readonly marker=${PEERCHALK_CUTOFF_MARKER:-/var/lib/peerchalk-turn/stopped-by-cutoff}

fail() {
    printf 'traffic-cutoff: %s\n' "$*" >&2
    exit 1
}

say() {
    printf 'traffic-cutoff: %s\n' "$*"
    if command -v logger >/dev/null; then logger -t peerchalk-traffic-cutoff -- "$*"; fi
}

whole_number() {
    [[ $2 =~ ^[0-9]+$ ]] || fail "$1 needs a whole number, not '$2'"
}

parse_arguments() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --stop-gb) stop_gb=${2:?--stop-gb needs a value}; shift 2 ;;
            --shutdown-gb) shutdown_gb=${2:?--shutdown-gb needs a value}; shift 2 ;;
            --interface) interface=${2:?--interface needs a value}; shift 2 ;;
            --check) check_gb=${2:?--check needs a value}; shift 2 ;;
            -h | --help) sed -n '6p' "$0" | sed 's/^# *//' >&2; exit 2 ;;
            *) fail "unknown argument: $1" ;;
        esac
    done
    whole_number --stop-gb "$stop_gb"
    whole_number --shutdown-gb "$shutdown_gb"
    [ -z "$check_gb" ] || whole_number --check "$check_gb"
    [ "$stop_gb" -gt 0 ] && [ "$shutdown_gb" -gt "$stop_gb" ] ||
        fail "--shutdown-gb must be above --stop-gb, and both above 0"
}

# What to do with this many GB sent this month: shutdown, stop or run.
decide() {
    if [ "$1" -ge "$shutdown_gb" ]; then
        echo shutdown
    elif [ "$1" -ge "$stop_gb" ]; then
        echo stop
    else
        echo run
    fi
}

# vnstat's one-line form, in bytes: field 8 is the month, field 10 its outgoing bytes.
sent_gb_this_month() {
    [ -n "$interface" ] ||
        interface=$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.* dev \([^ ]*\).*/\1/p')
    [ -n "$interface" ] || fail "could not tell the network interface; pass --interface"
    local line month bytes
    line=$(vnstat --oneline b -i "$interface") || fail "vnstat has no data for $interface yet"
    # A new database says so until its first save, a few minutes after vnstat starts.
    if [[ $line == *"No data."* ]]; then echo 0; return; fi
    month=$(cut -d';' -f8 <<<"$line")
    bytes=$(cut -d';' -f10 <<<"$line")
    [[ $month =~ ^[0-9]{4}-[0-9]{2}$ ]] || fail "vnstat's month '$month' is not YYYY-MM"
    [ "$month" = "$(date +%Y-%m)" ] || { echo 0; return; } # nothing sent yet this month
    whole_number "vnstat's byte count" "$bytes"
    echo $((bytes / 1000000000))
}

act() {
    local sent=$1
    case "$(decide "$sent")" in
        shutdown)
            say "$sent GB sent this month, at or over $shutdown_gb GB: powering off"
            mkdir -p "$(dirname "$marker")" && touch "$marker"
            systemctl poweroff
            ;;
        stop)
            if systemctl is-active --quiet coturn; then
                say "$sent GB sent this month, at or over $stop_gb GB: stopping coturn"
                mkdir -p "$(dirname "$marker")" && touch "$marker"
                systemctl stop coturn
            fi
            ;;
        run)
            if [ -e "$marker" ]; then
                say "$sent GB sent this month, under $stop_gb GB again: starting coturn"
                systemctl start coturn
                rm -f "$marker"
            fi
            ;;
    esac
}

main() {
    parse_arguments "$@"
    if [ -n "$check_gb" ]; then
        decide "$check_gb"
        return
    fi
    [ "$(id -u)" -eq 0 ] || fail "run it as root, or use --check"
    command -v vnstat >/dev/null || fail "vnstat is not installed"
    local sent
    sent=$(sent_gb_this_month)
    act "$sent"
}

main "$@"
