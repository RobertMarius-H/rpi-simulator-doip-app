#!/usr/bin/env bash
# Comenzi scurte pentru simulatorul Raspberry Pi. Se rulează din WSL.
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

compose() {
    docker compose "$@"
}

wsl_ip() {
    ip -4 -o addr show eth0 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | head -n1
}

print_connect_info() {
    local port="${SIM_PORT:-13400}"
    local ip
    ip="$(wsl_ip)"
    cat <<EOF

Simulatorul ascultă pe portul TCP ${port}.
În aplicația Windows (RpiDiag), la "Pi IPv4 address" folosiți:
  127.0.0.1          (recomandat, prin localhost forwarding din WSL)
  ${ip:-<IP WSL>}    (adresa WSL, dacă localhost nu merge)
Port: ${port}

Fișierele primite apar în: $(pwd)/data/image
EOF
}

case "${1:-}" in
    build)
        mkdir -p data/image
        compose build
        ;;
    up|start)
        mkdir -p data/image
        compose up -d --build
        print_connect_info
        ;;
    down|stop)
        compose down
        ;;
    restart)
        compose restart
        ;;
    logs)
        compose logs -f --tail=200
        ;;
    status|ps)
        compose ps
        ;;
    shell)
        # Deschide un terminal în "Pi-ul" simulat.
        docker exec -it rpi-sim bash
        ;;
    test)
        # Test rapid al conexiunii DoIP (routing activation + sesiuni UDS).
        python3 tools/doip_smoke_test.py "${2:-127.0.0.1}" "${3:-${SIM_PORT:-13400}}"
        ;;
    info|ip)
        print_connect_info
        ;;
    clean)
        compose down --rmi local
        rm -f data/image/*.img
        ;;
    *)
        cat <<'EOF'
Utilizare: ./sim.sh <comandă>

  build     construiește imaginea Docker (compilează serverul și rulează testele)
  up        construiește și pornește simulatorul în fundal
  down      oprește simulatorul
  restart   repornește simulatorul
  logs      afișează jurnalul serverului (Ctrl+C pentru ieșire)
  status    arată dacă rulează containerul
  shell     deschide bash în containerul "Pi"
  test      verifică rapid conexiunea DoIP: ./sim.sh test [host] [port]
  info      afișează adresa și portul pentru aplicația Windows
  clean     oprește, șterge imaginea Docker și fișierele primite

Variabile utile (pot fi puse în fișierul .env):
  SIM_PORT=13400                       portul expus pe host
  SIM_EXTRA_ARGS="--verification-min-ms 3000"
  RUN_TESTS=0                          sare peste testele C++ la build
  DOIP_DEMO_SECRET_HEX=<64 hex>        secret identic cu cel din aplicația Windows
EOF
        exit 2
        ;;
esac
