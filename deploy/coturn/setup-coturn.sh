#!/usr/bin/env bash
# Sets up a coturn TURN relay for PeerChalk on an Ubuntu server, such as an Oracle
# Cloud "Always Free" VM. Run it as root on the server:
#
#   sudo ./setup-coturn.sh [--public-ip ADDRESS] [--user NAME] [--password SECRET]
#                          [--realm NAME] [--print-config]
#
# It installs coturn, writes /etc/turnserver.conf (keeping a backup of any existing
# one), opens the TURN ports in the server's own firewall, limits what each IP address
# may use and starts the service. The relay is meant to be open: its credentials ship in
# the app, and the limits leave room for whiteboards but not for sustained video or bulk
# traffic. It finishes by printing the TURN setting for PeerChalk. The cloud provider's
# firewall (Oracle: the subnet's security list) must be opened separately; see README.md.
#
# --print-config only prints the configuration it would write, without root and
# without changing anything.
set -euo pipefail

readonly turn_port=3478
readonly relay_min_port=49160
readonly relay_max_port=49999
# Per IP address and direction. A busy whiteboard uses about 25 KB/s per person it reaches;
# the burst covers loading a board.
readonly ip_rate_kbytes=40
readonly ip_burst_mbytes=8
readonly ip_daily_mbytes=1024
readonly ip_max_flows=32
# The whole server: 3.5 MB/s is about 9 TB in 30 days, inside Oracle's 10 TB allowance.
readonly server_rate_kbytes=3500
readonly limits_file=/etc/peerchalk-turn-limits.nft

public_ip=""
user="peerchalk"
password="peerchalk-open-relay"
realm="peerchalk"
print_only=false

fail() {
    printf 'setup-coturn: %s\n' "$*" >&2
    exit 1
}

usage() {
    sed -n '5,6p' "$0" | sed 's/^# *//' >&2
    exit 2
}

parse_arguments() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --public-ip) public_ip=${2:?--public-ip needs a value}; shift 2 ;;
            --user) user=${2:?--user needs a value}; shift 2 ;;
            --password) password=${2:?--password needs a value}; shift 2 ;;
            --realm) realm=${2:?--realm needs a value}; shift 2 ;;
            --print-config) print_only=true; shift ;;
            -h | --help) usage ;;
            *) fail "unknown argument: $1" ;;
        esac
    done
}

# PeerChalk reads TURN settings as user:password@host:port, so neither may hold ':'
# or '@'; letters, digits, '-', '_' and '.' are always safe.
check_credentials() {
    [[ $user =~ ^[A-Za-z0-9._-]+$ ]] || fail "the user name may only use letters, digits, '.', '_' and '-'"
    [[ $password =~ ^[A-Za-z0-9._-]{16,}$ ]] ||
        fail "the password needs at least 16 letters, digits, '.', '_' or '-'"
}

# The address this server is reached at. Cloud VMs usually sit behind a 1:1 NAT, so
# the public address is not on any interface; ask Cloudflare's trace endpoint.
detect_addresses() {
    if [ -z "$public_ip" ]; then
        public_ip=$(curl -fsS --max-time 10 https://1.1.1.1/cdn-cgi/trace | sed -n 's/^ip=//p') ||
            true
    fi
    [[ $public_ip =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
        fail "could not tell the public IPv4 address; pass --public-ip"
    private_ip=$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.* src \([0-9.]*\).*/\1/p')
    [ -n "$private_ip" ] || private_ip=$public_ip
}

write_config() {
    local external="$public_ip"
    [ "$private_ip" = "$public_ip" ] || external="$public_ip/$private_ip"
    cat <<EOF
# PeerChalk TURN relay, written by setup-coturn.sh.
listening-port=$turn_port
external-ip=$external
min-port=$relay_min_port
max-port=$relay_max_port
realm=$realm

# One long-term user, as PeerChalk's settings expect.
lt-cred-mech
user=$user:$password
fingerprint

# Plain TURN over UDP and TCP only; PeerChalk relays over UDP.
no-tls
no-dtls
no-tcp-relay
no-cli
no-multicast-peers
no-software-attribute

# Never relay to this server's own network, the cloud metadata service or other
# private ranges, so the relay cannot be used to reach them.
denied-peer-ip=0.0.0.0-0.255.255.255
denied-peer-ip=10.0.0.0-10.255.255.255
denied-peer-ip=100.64.0.0-100.127.255.255
denied-peer-ip=127.0.0.0-127.255.255.255
denied-peer-ip=169.254.0.0-169.254.255.255
denied-peer-ip=172.16.0.0-172.31.255.255
denied-peer-ip=192.0.0.0-192.0.0.255
denied-peer-ip=192.168.0.0-192.168.255.255
denied-peer-ip=198.18.0.0-198.19.255.255
denied-peer-ip=240.0.0.0-255.255.255.255
denied-peer-ip=::1
denied-peer-ip=fc00::-fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
denied-peer-ip=fe80::-febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff

# Everyone shares one user, so limits here are per session and per server; the limits per
# IP address are in $limits_file.
total-quota=500
max-bps=$((ip_rate_kbytes * 1000))
bps-capacity=$((server_rate_kbytes * 1000))
stale-nonce=600

syslog
simple-log
EOF
}

# Rates are enforced in both directions, for the address on the far side of the server.
write_limits() {
    cat <<EOF
table inet peerchalk_turn
delete table inet peerchalk_turn
table inet peerchalk_turn {
    set rate_in { type ipv4_addr; flags dynamic, timeout; timeout 1h; }
    set rate_out { type ipv4_addr; flags dynamic, timeout; timeout 1h; }
    set day_in { type ipv4_addr; flags dynamic, timeout; timeout 1d; }
    set day_out { type ipv4_addr; flags dynamic, timeout; timeout 1d; }
    set flows { type ipv4_addr; flags dynamic; }

    chain input {
        type filter hook input priority filter - 5; policy accept;
        th dport { $turn_port, $relay_min_port-$relay_max_port } jump turn_in
    }
    chain output {
        type filter hook output priority filter - 5; policy accept;
        th sport { $turn_port, $relay_min_port-$relay_max_port } jump turn_out
    }
    chain turn_in {
        limit rate over $server_rate_kbytes kbytes/second drop
        ct state new add @flows { ip saddr ct count over $ip_max_flows } drop
        add @rate_in { ip saddr limit rate over $ip_rate_kbytes kbytes/second burst $ip_burst_mbytes mbytes } drop
        add @day_in { ip saddr quota over $ip_daily_mbytes mbytes } drop
    }
    chain turn_out {
        limit rate over $server_rate_kbytes kbytes/second drop
        add @rate_out { ip daddr limit rate over $ip_rate_kbytes kbytes/second burst $ip_burst_mbytes mbytes } drop
        add @day_out { ip daddr quota over $ip_daily_mbytes mbytes } drop
    }
}
EOF
}

install_limits() {
    write_limits >"$limits_file"
    cat >/etc/systemd/system/peerchalk-turn-limits.service <<EOF
[Unit]
Description=Per-address limits for the PeerChalk TURN relay
After=network-pre.target
Before=coturn.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/sbin/nft -f $limits_file
ExecStop=/usr/sbin/nft delete table inet peerchalk_turn

[Install]
WantedBy=multi-user.target
EOF
    systemctl daemon-reload
    systemctl enable --now peerchalk-turn-limits.service
}

install_coturn() {
    command -v apt-get >/dev/null || fail "this script expects Ubuntu or Debian (apt-get)"
    DEBIAN_FRONTEND=noninteractive apt-get update -q
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q coturn nftables curl iptables iproute2
    # Older packages start the service only when this is set.
    if [ -f /etc/default/coturn ]; then
        sed -i 's/^#\?TURNSERVER_ENABLED=.*/TURNSERVER_ENABLED=1/' /etc/default/coturn
    fi
}

install_config() {
    if [ -f /etc/turnserver.conf ]; then
        cp /etc/turnserver.conf "/etc/turnserver.conf.$(date +%Y%m%d%H%M%S).bak"
    fi
    write_config >/etc/turnserver.conf
    chown root:turnserver /etc/turnserver.conf 2>/dev/null || true
    chmod 640 /etc/turnserver.conf
}

# Adds an INPUT rule unless it is there already, ahead of any catch-all REJECT (Oracle's
# Ubuntu images end their INPUT chain with one).
allow_iptables() {
    local rule=(-p "$1" --dport "$2" -m conntrack --ctstate NEW -j ACCEPT)
    iptables -C INPUT "${rule[@]}" 2>/dev/null && return
    local reject
    reject=$(iptables -L INPUT --line-numbers -n | awk '$2 == "REJECT" {print $1; exit}')
    if [ -n "$reject" ]; then
        iptables -I INPUT "$reject" "${rule[@]}"
    else
        iptables -A INPUT "${rule[@]}"
    fi
}

open_firewall() {
    if command -v ufw >/dev/null && ufw status | grep -q '^Status: active'; then
        ufw allow "$turn_port/udp"
        ufw allow "$turn_port/tcp"
        ufw allow "$relay_min_port:$relay_max_port/udp"
        return
    fi
    command -v iptables >/dev/null || return 0
    allow_iptables udp "$turn_port"
    allow_iptables tcp "$turn_port"
    allow_iptables udp "$relay_min_port:$relay_max_port"
    if command -v netfilter-persistent >/dev/null; then
        netfilter-persistent save
    else
        echo "Note: install iptables-persistent to keep the firewall rules after a reboot." >&2
    fi
}

start_service() {
    systemctl enable coturn >/dev/null
    systemctl restart coturn
    sleep 2
    systemctl is-active --quiet coturn || fail "coturn did not start; see: journalctl -u coturn"
}

report() {
    cat <<EOF

coturn is running. Still to do in the Oracle Cloud console (Networking → Virtual cloud
networks → your VCN → Security Lists), add ingress rules from 0.0.0.0/0 for:
  UDP $turn_port, TCP $turn_port and UDP $relay_min_port-$relay_max_port

Then paste this into PeerChalk's Settings → Network → TURN server:

  $user:$password@$public_ip:$turn_port

This relay is open by design; each IP address is limited to $ip_rate_kbytes KB/s and
$ip_daily_mbytes MB per day in each direction.
EOF
}

main() {
    parse_arguments "$@"
    check_credentials
    $print_only || [ "$(id -u)" -eq 0 ] || fail "run it as root (sudo), or use --print-config"
    detect_addresses
    if $print_only; then
        write_config
        write_limits
        return
    fi
    install_coturn
    install_config
    open_firewall
    install_limits
    start_service
    report
}

main "$@"
