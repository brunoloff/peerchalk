#!/usr/bin/env bash
# Sets up an OpenDHT proxy for PeerChalk's browser version on an Ubuntu server, such as the
# Oracle Cloud VM that runs the TURN relay. Browsers cannot use the DHT directly; they find
# peers through a proxy like this one, over HTTPS. Run it as root on the server:
#
#   sudo ./setup-dht-proxy.sh [--hostname NAME]
#
# It installs dhtnode (a DHT node bootstrapped from Jami's, with its proxy on local port
# 8080) and Caddy, which serves the proxy over HTTPS with a Let's Encrypt certificate. The
# hostname defaults to the public IPv4 address as an sslip.io name, such as
# 89-168-35-4.sslip.io. The cloud provider's firewall (Oracle: the subnet's security list)
# must allow TCP 80 and 443 and UDP 4222; see README.md.
set -euo pipefail

readonly dht_port=4222
readonly proxy_port=8080
hostname=""

fail() {
    printf 'setup-dht-proxy: %s\n' "$*" >&2
    exit 1
}

parse_arguments() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --hostname) hostname=${2:?--hostname needs a value}; shift 2 ;;
            -h | --help) sed -n '6p' "$0" | sed 's/^# *//' >&2; exit 2 ;;
            *) fail "unknown argument: $1" ;;
        esac
    done
}

# The sslip.io name for this server's public address, which resolves to that address.
default_hostname() {
    local ip
    ip=$(curl -fsS --max-time 10 https://1.1.1.1/cdn-cgi/trace | sed -n 's/^ip=//p') || true
    [[ $ip =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] ||
        fail "could not tell the public IPv4 address; pass --hostname"
    echo "${ip//./-}.sslip.io"
}

install_packages() {
    command -v apt-get >/dev/null || fail "this script expects Ubuntu or Debian (apt-get)"
    DEBIAN_FRONTEND=noninteractive apt-get update -q
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q dhtnode caddy curl iptables
}

# The packaged service runs `dhtnode -s $DHT_ARGS` as its own user.
configure_dhtnode() {
    echo "DHT_ARGS=-b bootstrap.jami.net -p $dht_port --proxyserver $proxy_port" >/etc/default/dhtnode
    systemctl enable dhtnode >/dev/null
    systemctl restart dhtnode
}

# The proxy streams records to watchers, so Caddy must pass them on at once.
configure_caddy() {
    cat >/etc/caddy/Caddyfile <<EOF
# PeerChalk: the OpenDHT proxy that browsers find peers through.
$hostname {
	reverse_proxy 127.0.0.1:$proxy_port {
		flush_interval -1
	}
}
EOF
    caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile >/dev/null
    systemctl enable caddy >/dev/null
    systemctl restart caddy
}

# Adds an INPUT rule ahead of any catch-all REJECT (Oracle's Ubuntu images end with one).
# Port 8080 stays closed: only Caddy reaches the proxy.
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
        ufw allow 80/tcp
        ufw allow 443/tcp
        ufw allow "$dht_port/udp"
        return
    fi
    allow_iptables tcp 80
    allow_iptables tcp 443
    allow_iptables udp "$dht_port"
    if command -v netfilter-persistent >/dev/null; then
        netfilter-persistent save
    else
        echo "Note: install iptables-persistent to keep the firewall rules after a reboot." >&2
    fi
}

check_proxy() {
    local attempt
    for attempt in $(seq 1 30); do
        if curl -fsS --max-time 10 "https://$hostname/" >/dev/null 2>&1; then
            return
        fi
        sleep 2
    done
    fail "https://$hostname/ does not answer yet; see journalctl -u caddy and -u dhtnode (attempts: $attempt)"
}

main() {
    parse_arguments "$@"
    [ "$(id -u)" -eq 0 ] || fail "run it as root (sudo)"
    [ -n "$hostname" ] || hostname=$(default_hostname)
    install_packages
    configure_dhtnode
    open_firewall
    configure_caddy
    check_proxy
    cat <<EOF

The DHT proxy answers at https://$hostname. Put that address into PeerChalk's
Settings → Network → OpenDHT proxy in a browser.
EOF
}

main "$@"
