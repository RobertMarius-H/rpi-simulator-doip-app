#!/usr/bin/env bash
# Instalează Docker Engine + Docker Compose direct în WSL (Ubuntu), fără Docker Desktop.
# Pași preluați din documentația oficială: https://docs.docker.com/engine/install/ubuntu/
# Rulare: ./scripts/install-docker-wsl.sh   (cere parola sudo)
set -euo pipefail

if command -v docker >/dev/null 2>&1; then
    echo "Docker este deja instalat: $(docker --version)"
    exit 0
fi

. /etc/os-release
if [ "${ID:-}" != "ubuntu" ]; then
    echo "Scriptul este pentru Ubuntu; distribuția detectată: ${ID:-necunoscută}" >&2
    exit 1
fi

sudo apt-get update
sudo apt-get install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc

echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] \
https://download.docker.com/linux/ubuntu ${UBUNTU_CODENAME:-$VERSION_CODENAME} stable" \
    | sudo tee /etc/apt/sources.list.d/docker.list >/dev/null

sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io \
    docker-buildx-plugin docker-compose-plugin

# Permite rularea docker fără sudo.
sudo usermod -aG docker "$USER"

# WSL cu systemd pornește serviciul automat; altfel îl pornim manual.
if [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
    sudo systemctl enable --now docker
else
    sudo service docker start
fi

cat <<'EOF'

Docker a fost instalat.
IMPORTANT: închideți terminalul WSL și deschideți-l din nou (sau rulați "newgrp docker")
ca grupul "docker" să fie activ. Apoi verificați cu:  docker run --rm hello-world
EOF
