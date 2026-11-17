#!/bin/sh
# Pornește serverul DoIP ca pe Raspberry Pi.
# SIM_EXTRA_ARGS adaugă opțiuni, de ex. "--verification-min-ms 3000 --alive-idle-ms 5000".
set -eu

mkdir -p "$HOME/UpdMgr/image"

# Serverul refuză pornirea dacă variabila există dar e goală/invalidă.
# Compose o trimite goală când nu e configurată, deci o eliminăm în acest caz.
if [ -z "${DOIP_DEMO_SECRET_HEX:-}" ]; then
    unset DOIP_DEMO_SECRET_HEX
fi

echo "[rpi-sim] Server DoIP pornit în container $(hostname)"
echo "[rpi-sim] Port TCP: 13400  |  director imagini: $HOME/UpdMgr/image"
if [ -n "${SIM_EXTRA_ARGS:-}" ]; then
    echo "[rpi-sim] Opțiuni suplimentare: ${SIM_EXTRA_ARGS}"
fi

# shellcheck disable=SC2086  # împărțirea pe cuvinte este intenționată pentru opțiuni
exec /usr/local/bin/rpi_doip_server ${SIM_EXTRA_ARGS:-} "$@"
