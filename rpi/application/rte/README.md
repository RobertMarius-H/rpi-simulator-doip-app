# application/rte

`RteUpdate` este portul tipizat dintre Dcm și `UpdateManager`: serviciile UDS și
dispecerul cer operații de transfer, verificare, activare și anulare prin acest
contract. `UpdateManager` rămâne orchestratorul și nu interpretează UDS.

`RteStorage` este portul aplicației către `FirmwareStorage`. Managerii de update,
download și verificare folosesc acest port pentru operațiile de fișier; backendul
Linux păstrează descriptorii și căile. `main` configurează direct directorul
backendului la pornire, înainte de utilizarea RTE.

Acesta este un RTE-lite specific ECU-ului din proiect, nu un RTE AUTOSAR generat.
Rutarea porturilor nu adaugă politici de transfer sau copii ale imaginii în RAM.
