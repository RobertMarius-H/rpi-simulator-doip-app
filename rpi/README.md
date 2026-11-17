# Raspberry Pi server

Linux C++ application for the Pi 3 B+ target. It should remain portable enough to run on another Linux Pi for development. The intended boundaries are DoIP connection/framing, UDS dispatcher/session/security, transfer FSM, file storage, and verification. See [architecture](../docs/architecture.md).

Executabilul ascultă TCP pe portul configurat, primește activarea rutei DoIP, confirmă un mesaj diagnostic și procesează UDS `0x10`, `0x27`, `0x31`, `0x34`, `0x36` și `0x37`. Pentru SecurityAccess, intră mai întâi în sesiunea de programare (`10 02`), apoi cere seed (`27 01`) și trimite cheia HMAC-SHA-256 (`27 02` + 32 octeți). La 15 noiembrie 2026, cele 15 teste GoogleTest au trecut pe x86-64; binarul actual necesită validare pe Pi fizic. Se poate folosi `--port <1..65535>` pentru testare locală; implicit se folosește portul `13400`.

Pentru demonstrație, serverul și aplicația Windows folosesc implicit aceeași valoare de test, definită în cod. Ambele pot citi `DOIP_DEMO_SECRET_HEX` din propriul mediu; valoarea trebuie să conțină exact 64 de caractere hexazecimale și să fie identică pe PC și Pi. Serverul refuză pornirea dacă variabila este prezentă și invalidă; clientul refuză deblocarea. Configurați variabila în afara Git înainte de utilizare în afara testelor. Valoarea implicită rămâne inclusă în binar și nu oferă protecție reală. Blocarea după trei chei greșite durează 60 de secunde și continuă peste reconectări cât timp procesul serverului rulează.

Această felie ascultă pe toate interfețele Linux și permite un singur tester diagnostic activ. La cererea unui al doilea tester valid, serverul trimite primului un alive-check DoIP. Dacă răspunde în trei secunde, noul tester primește codul de rutare `0x01`; dacă nu, serverul închide ruta veche, resetează starea diagnostică și acceptă noua activare. Cererile de rutare cu adresă sau tip invalid sunt respinse fără a perturba testerul activ. După 60 de secunde de inactivitate, conexiunea activă primește un alive-check. Pentru probe rapide, `--alive-idle-ms <100..60000>` poate reduce temporar acest interval. Stagingul scrie `incoming.img` în `~/UpdMgr/image` (rezolvat din `HOME`); pentru teste, directorul poate fi schimbat cu `--update-dir <path>`. Directorul trebuie să existe și să fie accesibil la pornire. `0x31/F001` calculează SHA-256 într-un worker și trimite `7F 31 78` până la răspunsul final. `0x31/F003` permite interogarea procentului prin `31 03 F0 03`; imaginea validă devine `verified.img`. `0x31/F002` șterge fișierul verificat, revine la sesiunea implicită și curăță SecurityAccess, păstrând ruta DoIP. Pentru o demonstrație în care verificarea durează cel puțin trei secunde, porniți `./rpi_doip_server --verification-min-ms 3000`. Alte servicii UDS primesc `7F <SID> 11`.

## C++ coding style

Name constants in `UPPER_SNAKE_CASE`; use `enum class` for numeric values and `constexpr` for strings (for example, `DEFAULT_TCP_PORT` and `LOGICAL_ADDRESS_BYTES`).

Clasa `Socket` izolează apelurile POSIX, `SoAd` expune transportul, `DoipProtocol` încadrează fluxul TCP, `DoipAdmission` validează intrarea, `DoipRouting` răspunde la activare, `DoipConnection` controlează ruta activă, `PduRouter` validează ruta diagnosticului, iar `DcmSession`, `UdsDispatcher` și `UdsSecurity` procesează serviciile implementate. `RteUpdate` este portul dintre Dcm și `UpdateManager`, iar `RteStorage` este portul dintre aplicație și `FirmwareStorage`. `UpdateManager` orchestrează fazele aplicației, `DownloadManager` deține transferul, iar `VerificationManager` deține workerul de hash. Fiecare clasă are un singur header, propriul `.cpp` și acces prin `getInstance()`; `main` configurează serviciile care au nevoie de valori la pornire; celelalte singletonuri se construiesc la prima utilizare. [Arhitectura țintă](../docs/architecture/README.md) și [fluxul OTA propus](../docs/architecture/update-pipeline.md) separă implementarea actuală de extensiile planificate.

## Containerized WSL/Linux development

Only Docker is needed on the WSL/Linux host. The Debian 12 image in [docker/Dockerfile](docker/Dockerfile) contains the C++ toolchain, Make, CMake, Ninja, GDB, Git, GoogleTest, GoogleMock, and OpenSSL development headers.

## Open a shell and build

Run these commands in WSL, from the repository root. `image` creates the image the first time, or rebuilds it after the Dockerfile changes. `shell` starts a temporary container and opens its Bash CLI:

```sh
./rpi/dev.sh image
./rpi/dev.sh shell
```

The prompt is now **inside the container**, at `/workspace`. Build the application there:

```sh
cmake --fresh -S rpi -B rpi/build -DCMAKE_BUILD_TYPE=Debug
cmake --build rpi/build --parallel
./rpi/build/rpi_doip_server
ctest --test-dir rpi/build --output-on-failure
exit
```

`exit` closes the container shell and returns to WSL. The same build can be run without opening a shell:

```sh
./rpi/dev.sh configure
./rpi/dev.sh build
./rpi/dev.sh test
```

`test` rulează testele C++ GoogleTest: decodificarea hexazecimală și integrarea DoIP/UDS prin socket local. GoogleMock este disponibil pentru viitoare limite care justifică simularea.

Pentru raportul LCOV pe x86-64, reconstruiți imaginea după modificarea Dockerfile-ului și rulați:

```sh
./rpi/dev.sh image
./rpi/dev.sh coverage
```

Raportul este în [`build-coverage/html/index.html`](build-coverage/html/index.html), iar datele filtrate sunt în `build-coverage/coverage.info`. Scriptul reconstruiește și testează un build separat cu instrumentare GCC; exclude codul testelor din numitor. Ultimul raport, generat înaintea simplificării curente și încă neregenerat, a raportat **89,7% linii (477/532)** și **100% funcții (66/66)** și **56,5% ramuri (307/543)** pentru codul Pi. Acoperirea arată doar execuția actualelor teste, nu conformitate sau completitudinea protocolului.

## How the container is structured

| Piece | Role |
| --- | --- |
| `rpi/docker/Dockerfile` | Recipe for the Debian 12 image and installed development packages |
| `psi-rpi-dev:bookworm` | Local x86-64 image built by `./rpi/dev.sh image` on an x86-64 WSL host |
| `rpi/dev.sh` | Wrapper for image builds and short-lived `docker run` commands |
| `/workspace` | The repository mounted from WSL; edits appear on both sides immediately |
| `/workspace/rpi/build` | Native build output, stored in the repository folder and ignored by Git |
| `psi-rpi-dev:bookworm-arm64` | Separate ARM64 image used to build for the 64-bit Pi |
| `/workspace/rpi/build-arm64` | ARM64 build output, also ignored by Git |

The wrapper mounts the repository and runs as your WSL user ID so generated files remain editable. Containers are removed when commands finish; the images and files in the mounted repository remain. No compiler, CMake, GoogleTest, or GoogleMock installation is required in WSL itself.

To check what is installed while inside the shell, use `g++ --version`, `cmake --version`, or `dpkg-query -W make libgtest-dev libgmock-dev libssl-dev`.

## Build for the Raspberry Pi

The Pi will use 64-bit Linux and will **not** run Docker. An x86-64 WSL build produces an x86-64 executable, so build a second ARM64 image through Docker Buildx when preparing a Pi binary. This requires a Docker installation with Buildx and ARM64 emulation enabled; that path was validated in the current WSL environment.

```sh
./rpi/dev.sh image-arm64
./rpi/dev.sh configure-arm64
./rpi/dev.sh build-arm64
./rpi/dev.sh test-arm64
file rpi/build-arm64/rpi_doip_server
```

To work interactively in that image, run `./rpi/dev.sh shell-arm64` after `image-arm64`. Inside its shell, use `cmake --fresh -S rpi -B rpi/build-arm64 -DCMAKE_BUILD_TYPE=Release` and `cmake --build rpi/build-arm64 --parallel`, then `exit`.

Imaginea ARM64 rulează sub emulare în WSL, mai lent decât pe hardware. Executabilul actual a trecut 17/17 teste GoogleTest ARM64 la 15 noiembrie 2026, inclusiv intercalarea unei cereri UDS cu alive-check, 20 de tentative succesive ale unui al doilea tester și expirarea unui alive-check fără răspuns; această versiune necesită încă validare pe Pi fizic. Imaginea folosește Debian 12 pentru compatibilitate cu un Pi OS bazat pe Debian 12; bibliotecile runtime de pe Pi trebuie să fie compatibile.

GoogleTest este legat în CMake pentru testele de unitate și de integrare.
