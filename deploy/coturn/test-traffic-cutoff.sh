#!/usr/bin/env bash
# Tests traffic-cutoff.sh with stand-ins for vnstat, systemctl, id and logger, changing
# nothing on this machine. Run: ./test-traffic-cutoff.sh
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failures=0

# The stand-ins: vnstat reports $SENT_BYTES for $MONTH; systemctl records its calls and
# keeps coturn's state in a file.
mkdir "$work/bin"
cat >"$work/bin/vnstat" <<'EOF'
#!/usr/bin/env bash
if [ "$MONTH" = new ]; then
    echo " eth0: No data. Timestamp of last update is same 2026-11-22 18:08:56 as of database creation."
else
    echo "1;eth0;2026-11-22;0;0;0;0;${MONTH};0;${SENT_BYTES};0;0;0;0;0"
fi
EOF
cat >"$work/bin/systemctl" <<'EOF'
#!/usr/bin/env bash
echo "$*" >>"$WORK/calls"
case "$1" in
    is-active) [ "$(cat "$WORK/coturn")" = active ] ;;
    stop) echo inactive >"$WORK/coturn" ;;
    start) echo active >"$WORK/coturn" ;;
esac
EOF
printf '#!/bin/sh\necho 0\n' >"$work/bin/id"
printf '#!/bin/sh\n:\n' >"$work/bin/logger"
chmod +x "$work/bin/"*

expect() {
    if [ "$2" = "$3" ]; then
        printf 'ok   %s\n' "$1"
    else
        printf 'FAIL %s: expected [%s], got [%s]\n' "$1" "$2" "$3"
        failures=$((failures + 1))
    fi
}

# Runs the cutoff with GB sent this month and coturn in the given state; prints the
# systemctl calls other than is-active.
run_with() {
    : >"$work/calls"
    echo "$2" >"$work/coturn"
    PATH="$work/bin:$PATH" WORK="$work" MONTH="${3:-$(date +%Y-%m)}" \
        SENT_BYTES="$(($1 * 1000000000))" PEERCHALK_CUTOFF_MARKER="$work/marker" \
        "$here/traffic-cutoff.sh" --interface eth0 >/dev/null || return 1
    grep -v '^is-active' "$work/calls" | paste -sd, - || true
}

expect "under half runs" run "$("$here/traffic-cutoff.sh" --check 4999)"
expect "half stops" stop "$("$here/traffic-cutoff.sh" --check 5000)"
expect "80% powers off" shutdown "$("$here/traffic-cutoff.sh" --check 8000)"
expect "own thresholds" stop "$("$here/traffic-cutoff.sh" --stop-gb 10 --shutdown-gb 20 --check 15)"
expect "bad threshold refused" refused \
    "$("$here/traffic-cutoff.sh" --stop-gb 9 --shutdown-gb 5 --check 1 2>/dev/null || echo refused)"
expect "bad number refused" refused \
    "$("$here/traffic-cutoff.sh" --check 1.5 2>/dev/null || echo refused)"

rm -f "$work/marker"
expect "quiet month leaves coturn alone" "" "$(run_with 100 active)"
expect "stops coturn at half" "stop coturn" "$(run_with 5200 active)"
expect "does not stop it twice" "" "$(run_with 5300 inactive)"
expect "restarts it in a new month" "start coturn" "$(run_with 0 inactive 2026-01)"
expect "leaves a coturn stopped by hand" "" "$(run_with 10 inactive)"
expect "powers off at 80%" poweroff "$(run_with 8100 active)"
rm -f "$work/marker"
expect "a new database counts as nothing sent" "" "$(run_with 0 active new)"
expect "an unreadable month is an error" refused \
    "$(run_with 9000 active 11/2026 2>/dev/null || echo refused)"

[ "$failures" -eq 0 ] || { echo "$failures failed"; exit 1; }
echo "all passed"
