# platform/linux/storage

`FirmwareStorage` deține descriptorul directorului de actualizări și fișierul
`incoming.img`. Scrie blocuri limitate și sincronizează datele la fiecare
4 MiB și la finalizarea cererii `0x37`. Confirmarea `0x36` indică acceptarea
blocului prin `write()`, iar răspunsul `0x37` indică și sincronizarea finală.
Curăță fișierele rămase la pornire. Performanța după această optimizare nu a
fost încă măsurată pe Pi.
