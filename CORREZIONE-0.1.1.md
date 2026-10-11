# Correzione avvio audio 0.1.1

Il log della scheda mostra un abort FreeRTOS durante setup e successivo safe mode.
Il YAML 0.1.0 attivava sound_level dal ripristino dello switch audio, prima del setup
I2S: start() usa un semaforo ancora nullo. Inoltre avviava di nuovo sound_level da
on_boot, anche se passive: false lo avvia automaticamente nel suo setup.

La versione 0.1.1 lascia il primo avvio al setup ufficiale sound_level. Le azioni
dello switch possono avviare/fermare il microfono solo dopo on_boot a priorità -100.
Il trigger audio mantiene il valore iniziale acceso.

Sostituire birdcam.yaml mantenendo i propri secrets, compilare e installare.
Se la scheda è raggiungibile in safe mode, usare OTA; altrimenti installare via USB.
La correzione è verificata sullo schema e sul codice ESPHome 2026.9.1; resta da
confermare il nuovo avvio sulla scheda. Il backtrace non è stato decodificato perché
non è disponibile il firmware.elf esatto della compilazione del log.
