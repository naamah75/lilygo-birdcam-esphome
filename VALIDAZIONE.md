# Verifica del progetto — 0.2.0 WAV sperimentale

Target: LILYGO T-Camera S3 V1.6 con microfono I2S, ESPHome **2026.9.1**,
framework ESP-IDF **5.5.5**. L'avvio della versione precedente è stato confermato
dall'utente; la nuova registrazione a 48 kHz resta da provare sulla scheda.

## Controlli completati

- Validazione locale della configurazione completa e generazione C++.
- Rifiuto di una porta foto maggiore di 55535 e di un riferimento camera errato.
- Compilazione completa su Linux in GitHub Actions, inclusi nuovi componenti,
  collegamento del firmware e generazione di factory.bin, OTA ed ELF.
- Test C++ del registratore con API ESPHome/HTTP/microfono simulate: WAV PCM
  mono 48 kHz/16 bit letto dal modulo Python wave, funzione spenta di default,
  filtro delle modalità, cooldown condiviso, assenza di coda, annullamento,
  durata immutabile della clip in corso, errore di memoria, timeout,
  blocchi rifiutati e download della vecchia clip durante la sostituzione.
- Pinout e tensioni PMU confrontati con i sorgenti del produttore.

Compilazione firmware riuscita:
[GitHub Actions #2](https://github.com/naamah75/lilygo-birdcam-esphome/actions/runs/38132227232),
commit `a76a237d939dacb9f613efab954017dc8fccdd2e`.
Test C++ riusciti nello step dedicato della
[verifica #3](https://github.com/naamah75/lilygo-birdcam-esphome/actions/runs/38132332006).
I file del firmware sono uguali nelle due verifiche; il terzo commit aggiunge
test host e aggiornamento del workflow.

Il report del linker indica RAM statica **129319 / 341760 byte (37,8%)** e
flash applicazione **1036515 / 8126464 byte (12,8%)**. Questi valori non includono
le allocazioni dinamiche della PSRAM né dimostrano il margine durante video/audio.

La CI usa credenziali Wi-Fi dimostrative e una chiave API casuale temporanea.
I binari CI non vengono distribuiti né devono essere installati sulla scheda:
ricompilare con i propri secrets in Device Builder.

## Prove hardware ancora necessarie

1. Modalità iniziale Spento e nessuna registrazione automatica.
2. Solo manuale: WAV di sei secondi, valido e ascoltabile da `/last.wav`.
3. Audio/PIR: massimo un avvio ogni 60 secondi; eventi intermedi ignorati.
4. Foto e video durante registrazione e download WAV, senza riavvii.
5. Audio senza interruzioni a 48 kHz, PSRAM e heap stabili dopo clip ripetute.
6. Cambio a Spento durante registrazione: clip incompleta scartata, precedente
   conservata; microfono fermato se il trigger foto audio è anch'esso spento.
7. USB/batteria, display, cambio risoluzione e aggiornamento OTA.
8. Riavvio: foto e WAV precedenti assenti, perché conservati solo in PSRAM.

Le API simulate nei test host non verificano driver I2S, DMA, camera, scheduling
ESP32 o assorbimento. Nessuna stima dell'autonomia o percentuale di CPU misurata.
Il caricatore resta configurato come prima, finché la batteria non è identificata.

## Limiti dell'ambiente locale

Il tentativo iniziale di compilazione Windows era bloccato dall'avvio del
compilatore Xtensa (WinError 623). Anche il compilatore host Zig non riesce a
risolvere il proprio percorso eseguibile nel sandbox locale. I controlli C++
e la compilazione firmware sono stati quindi eseguiti su GitHub Actions/Linux.
