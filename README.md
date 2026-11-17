# rpi-simulator

Simulator pentru Raspberry Pi-ul din proiectul PSI. Rulează **același server DoIP/UDS** (`rpi_doip_server`, C++) ca Pi-ul real, într-un container Docker cu Ubuntu, în WSL 2. Aplicația de diagnoză de pe Windows (**RpiDiag**) se conectează la el exact ca la un Pi legat prin Ethernet. Așa se poate testa tot fluxul fără placă: conectare, sesiuni UDS, deblocarea securității, transferul imaginii (dummy file), verificarea și activarea simulată.

---

## Tutorial rapid

### Pasul 0 – Cerințe

- Windows 10/11 cu **WSL 2** și o distribuție **Ubuntu** (de ex. Ubuntu 24.04).
  Dacă nu aveți WSL, rulați în PowerShell ca administrator: `wsl --install -d Ubuntu-24.04`, apoi reporniți calculatorul.
- Aplicația **RpiDiag** (din repository-ul `eth-connector`), deschisă în Visual Studio cu .NET 10.
- Python 3 pe Windows, doar pentru testul rapid de conexiune (opțional).

### Pasul 1 – Instalați Docker în WSL (o singură dată)

Deschideți un terminal (cmd sau PowerShell) în folderul acestui proiect și intrați în WSL:

```bash
wsl
```

Promptul devine ceva de genul `user@PC:/mnt/c/.../rpi-simulator$`. Rulați scriptul de instalare (cere parola de Ubuntu; în timp ce o scrieți nu apare nimic pe ecran, e normal):

```bash
bash scripts/install-docker-wsl.sh
```

Ieșiți din WSL cu `exit`.

### Pasul 2 – Opriți închiderea automată a WSL (o singură dată)

Implicit, WSL se oprește la aproximativ un minut după ce se închide ultima fereastră WSL. Odată cu el se opresc Docker și simulatorul, iar RpiDiag afișează *„target machine actively refused it”*.

Creați fișierul `C:\Users\<utilizator>\.wslconfig` cu acest conținut:

```ini
[general]
instanceIdleTimeout=-1

[wsl2]
vmIdleTimeout=-1
```

Apoi aplicați setarea, din cmd sau PowerShell:

```bash
wsl --shutdown
```

### Pasul 3 – Porniți simulatorul

Din folderul proiectului, în **cmd sau PowerShell**:

```bash
.\sim up
```

Prima rulare durează câteva minute: descarcă Ubuntu, compilează serverul și rulează cele 17 teste C++. Următoarele porniri durează câteva secunde. La final apare:

```
Simulatorul ascultă pe portul TCP 13400.
În aplicația Windows (RpiDiag), la "Pi IPv4 address" folosiți:
  127.0.0.1
Port: 13400
```

### Pasul 4 – Verificați conexiunea (opțional)

```bash
.\sim test
```

Trebuie să vedeți `OK` la TCP, Routing activation, `UDS 10 01`, `UDS 10 02` și `UDS 27 01`.

### Pasul 5 – Conectați RpiDiag

1. Porniți RpiDiag din Visual Studio.
2. În tab-ul **Connection** completați:
   - **Pi IPv4 address:** `127.0.0.1`
   - **TCP port:** `13400`
3. Apăsați **Connect**; indicatorul devine verde.

### Pasul 6 – Trimiteți o imagine

1. Generați un fișier de test, de exemplu 300 MB, în cmd:
   ```bash
   fsutil file createnew "%USERPROFILE%\Desktop\dummy300mb.bin" 314572800
   ```
2. În RpiDiag, tab-ul **Diagnostics**: apăsați **Programming session**, apoi **Unlock security**.
3. Alegeți fișierul și apăsați **Stage and verify**.
4. Fișierul ajunge în folderul `data\image\` al acestui proiect: întâi ca `incoming.img`, iar după verificare ca `verified.img`.
5. Opțional, comparați SHA-256 pentru fișierul trimis și cel primit. Cele două valori trebuie să fie identice:
   ```bash
   certutil -hashfile "%USERPROFILE%\Desktop\dummy300mb.bin" SHA256
   ```
   ```bash
   certutil -hashfile data\image\verified.img SHA256
   ```

### Pasul 7 – Opriți simulatorul

```bash
.\sim down
```

Fișierele din `data\image\` rămân. După un restart al calculatorului rulați doar `.\sim up`.

---

## Comenzi

Toate comenzile se rulează din folderul proiectului. Pe Windows (cmd sau PowerShell) se folosește `.\sim <comandă>`; în WSL, `bash sim.sh <comandă>`.

| Comandă | Ce face |
| --- | --- |
| `.\sim up` | construiește imaginea (dacă e nevoie) și pornește simulatorul în fundal |
| `.\sim down` | oprește simulatorul (fișierele primite rămân) |
| `.\sim restart` | repornește serverul |
| `.\sim status` | arată dacă rulează containerul |
| `.\sim logs` | jurnalul serverului în timp real (Ctrl+C pentru ieșire) |
| `.\sim test [host] [port]` | test rapid DoIP de pe Windows (implicit `127.0.0.1 13400`) |
| `.\sim info` | afișează adresele și portul pentru RpiDiag |
| `.\sim shell` | deschide un terminal bash în „Pi”, de ex. `ls -l ~/UpdMgr/image` |
| `.\sim build` | doar reconstruiește imaginea Docker |
| `.\sim clean` | oprește simulatorul, șterge imaginea Docker și fișierele `.img` primite |

> În cmd nu rulați `.\sim.ps1` direct: Windows îl deschide în Notepad. Folosiți `.\sim`, care pornește `sim.cmd` și merge și în cmd, și în PowerShell.

---

## Cum funcționează

### Arhitectură

```
 Windows                                    WSL 2 (Ubuntu)
┌───────────────────────┐   TCP 13400   ┌──────────────────────────────────────┐
│ RpiDiag (WPF)         │──────────────►│ Docker Engine                        │
│ Pi IPv4: 127.0.0.1    │  localhost    │  ┌────────────────────────────────┐  │
│ Port:    13400        │  forwarding   │  │ container "rpi-sim"            │  │
└───────────────────────┘               │  │ Ubuntu 24.04 minimal, user pi  │  │
                                        │  │ rpi_doip_server  :13400        │  │
 C:\...\rpi-simulator\data\image ◄──────┼──┤ /home/pi/UpdMgr/image (mount)  │  │
                                        │  └────────────────────────────────┘  │
                                        └──────────────────────────────────────┘
```

1. **Containerul** rulează `rpi_doip_server`, compilat din aceleași surse ca pe Pi, ca utilizatorul `pi`, cu hostname-ul `raspberrypi`.
2. **Docker** publică portul `13400` al containerului pe portul `13400` al WSL.
3. **WSL 2** redirecționează automat porturile din Linux către `localhost` pe Windows (*localhost forwarding*). De aceea RpiDiag se conectează la `127.0.0.1`. Funcționează și IP-ul WSL (`172.x.x.x`, afișat de `.\sim info`), dar acesta se poate schimba după un restart.
4. **Folderul `data\image`** de pe Windows este montat în container ca `/home/pi/UpdMgr/image`, directorul în care serverul scrie imaginea. Nu se face nicio copie: serverul scrie direct pe discul Windows, deci fișierele rămân și după oprirea containerului.

### Structura proiectului

| Fișier / folder | Rol |
| --- | --- |
| `rpi/` | Codul sursă C++ al serverului de pe Pi (copie din `doip-prototype`), nemodificat |
| `Dockerfile` | Build în două etape: compilare și teste, apoi imagine de runtime minimală |
| `docker/entrypoint.sh` | Pornește serverul în container și citește opțiunile din mediu |
| `docker-compose.yml` | Definește containerul `rpi-sim`: port, volum, variabile |
| `sim.sh` | Comenzile simulatorului (rulează în WSL) |
| `sim.cmd`, `sim.ps1` | Wrapper-e Windows care rulează `sim.sh` în WSL |
| `tools/doip_smoke_test.py` | Client DoIP minimal pentru testul de conexiune (doar biblioteca standard Python) |
| `scripts/install-docker-wsl.sh` | Instalează Docker Engine + Compose în Ubuntu (după documentația oficială Docker) |
| `data/image/` | Imaginile primite de „Pi” (ignorate de Git) |
| `.env.example` | Model pentru configurarea locală |

### Imaginea Docker

`Dockerfile` folosește două etape:

1. **build** (`ubuntu:24.04` + `build-essential`, `cmake`, `ninja`, `libssl-dev`, GoogleTest): compilează serverul în `Release` și rulează `ctest`. Dacă un test pică, build-ul se oprește, deci nu poate porni un simulator stricat.
2. **runtime** (`ubuntu:24.04` + doar `libssl3`): conține numai executabilul `rpi_doip_server`, utilizatorul `pi` și directorul `~/UpdMgr/image`. Expune portul `13400/tcp`.

`docker-compose.yml` adaugă:
- `init: true`, ca serverul să primească semnalul de oprire și `.\sim down` să fie instantaneu;
- `restart: unless-stopped`, ca simulatorul să repornească automat odată cu Docker, până la un `.\sim down` explicit.

### Ce face serverul

Serverul este cel din proiectul principal; detaliile complete sunt în `rpi/README.md`. Pe scurt:

| Pas | Mesaj | Rol |
| --- | --- | --- |
| Routing activation | DoIP `0x0005` → `0x0006` | tester `0x0E80` → Pi `0x1000`, cod `0x10` = activat |
| Sesiune | UDS `10 01` / `10 02` | sesiune implicită / de programare |
| Securitate | UDS `27 01` / `27 02` | seed de 16 octeți, cheie HMAC-SHA-256 |
| Transfer | UDS `34` / `36` / `37` | cerere de download, blocuri de max. 16.384 octeți, final |
| Verificare | UDS `31 01 F0 03` + `31 03 F0 03` | SHA-256 cu procent de progres |
| Activare simulată | UDS `31 01 F0 02` | șterge imaginea verificată |
| Menținere conexiune | DoIP alive-check `0x0007` / `0x0008` | după 60 s de inactivitate |

### Ciclul de viață al fișierelor din `data/image`

| Moment | Fișier |
| --- | --- |
| În timpul transferului | `incoming.img` (crește treptat) |
| Verificare SHA-256 reușită | redenumit în `verified.img` |
| Deconectare / Cancel / schimbare de sesiune înainte de verificare | `incoming.img` este șters |
| **Simulate activation** | `verified.img` este șters |
| Transfer nou | fișierul anterior este înlocuit |
| `.\sim down`, `wsl --shutdown`, restart PC | fișierele **rămân** |
| `.\sim clean` | fișierele `.img` sunt șterse |

---

## Configurare

Copiați `.env.example` în `.env` și modificați valorile. După orice modificare rulați din nou `.\sim up`.

| Variabilă | Implicit | Rol |
| --- | --- | --- |
| `SIM_PORT` | `13400` | portul de pe host la care se conectează RpiDiag |
| `SIM_BIND_ADDRESS` | `0.0.0.0` | `127.0.0.1` limitează accesul doar la WSL |
| `SIM_EXTRA_ARGS` | gol | opțiuni pentru server, de ex. `--verification-min-ms 3000` (verificare lentă pentru demo) sau `--alive-idle-ms 5000` |
| `RUN_TESTS` | `1` | `0` sare peste testele C++ la build |
| `DOIP_DEMO_SECRET_HEX` | gol | secretul HMAC (64 caractere hex). Gol = secretul demo din cod, identic cu cel din RpiDiag. Dacă îl setați, setați aceeași variabilă de mediu și pe Windows pentru RpiDiag |

`.env` este ignorat de Git, pentru că poate conține secretul.

---

## Depanare

| Problemă | Cauză / soluție |
| --- | --- |
| RpiDiag: *„target machine actively refused it”* | Simulatorul nu rulează. Rulați `.\sim status`; dacă nu apare `Up`, rulați `.\sim up`. Dacă se oprește singur după un minut, configurați `.wslconfig` (Pasul 2). |
| `.\sim.ps1` se deschide în Notepad | Sunteți în cmd. Folosiți `.\sim up`. |
| PowerShell: *„running scripts is disabled”* | Folosiți `.\sim up` (`sim.cmd` ocolește restricția) sau rulați o dată `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`. |
| `127.0.0.1` nu merge, dar simulatorul rulează | Localhost forwarding e dezactivat în WSL. Folosiți IP-ul afișat de `.\sim info`. |
| `docker: permission denied` în WSL | După instalare, utilizatorul trebuie să intre din nou în grupul `docker`: rulați `wsl --shutdown` și reporniți. |
| *„Unlock security”* eșuează | `DOIP_DEMO_SECRET_HEX` diferă între PC și simulator. Lăsați-l gol pe ambele sau setați aceeași valoare. |
| Port 13400 ocupat | Setați alt `SIM_PORT` în `.env` și folosiți-l în RpiDiag. |

Jurnalul serverului: `.\sim logs`. Jurnalul RpiDiag: `Documents\PSI\Logs\`.

---

## Diferențe față de Pi-ul real

- Arhitectura este **x86-64** în loc de ARM64. Codul sursă este același, dar e compilat nativ pentru PC.
- Conexiunea trece prin rețeaua virtuală WSL/Docker, nu printr-un cablu Ethernet, deci transferul este mai rapid decât pe Pi.
- Imaginile sunt scrise pe discul Windows (`data/image`), nu pe cardul SD.

Simulatorul nu înlocuiește testarea finală pe Pi, dar permite dezvoltarea și demonstrarea aplicației Windows fără placă.

---

## Actualizarea serverului

`rpi/` este o copie a folderului `rpi/` din repository-ul `doip-prototype`. Când serverul se modifică:

1. Înlocuiți conținutul folderului `rpi/` din acest proiect cu versiunea nouă (fără folderele `build*`).
2. Rulați `.\sim up`. Imaginea se reconstruiește și testele rulează din nou.
