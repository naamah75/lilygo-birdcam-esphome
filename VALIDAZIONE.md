# Verifica del progetto

Versione di riferimento: ESPHome **2026.9.1**, stabile verificata tramite la
release ufficiale GitHub. Hardware target: LILYGO T-Camera S3 V1.6 con microfono I²S.

## Completato

- Caricamento del componente tramite `external_components` con sorgente locale.
- Validazione della configurazione completa con il comando `esphome config`.
- Controllo che una porta foto superiore a 55535 venga rifiutata (la porta di
  controllo HTTP è porta foto + 10000).
- Controllo che un riferimento a una camera inesistente venga rifiutato.
- Generazione C++ e predisposizione delle dipendenze IDF del progetto.
- Verifica nel sorgente ESPHome 2026.9.1 dei listener camera, richieste immagini,
  metodi OLED, API I²S/sound_level e dichiarazione dei socket necessari.
- Verifica del pinout e delle tensioni ALDO/BLDO nei sorgenti LILYGO.

## Compilazione e hardware

**Compilazione completa non confermata.** Il tentativo locale ha raggiunto
l'installazione ESP-IDF 5.5.5 e del compilatore Xtensa, poi si è fermato prima
della compilazione del progetto. Windows non riesce ad avviare il compilatore
`xtensa-esp-elf` 14.2.0_20260121 e restituisce **WinError 623**, errore di
rilocazione/caricamento di una DLL di sistema. Non è un diagnostico C++ del
componente. Non sono state modificate le impostazioni del sistema operativo.

La configurazione finale ha superato nuovamente i tre controlli dello schema.
La generazione C++ finale è stata eseguita con ESPHome 2026.9.1. Il firmware
deve essere compilato in Device Builder prima dell'installazione: la validazione
YAML e la generazione non equivalgono alla compilazione/link del firmware.
Non viene fornito un binario con credenziali dimostrative.

Non è stata eseguita una prova sulla scheda. Prima di usarla stabilmente:

1. Avvio su USB: log PMU, PSRAM, camera e microfono senza errori.
2. Pulsante Scatta foto: JPEG valido da `/last.jpg`, aggiornamento contatore.
3. PIR e audio funzionanti anche quando HA non è collegato.
4. Download foto durante nuovi scatti: file completo, senza JPEG corrotti.
5. Cambio di tutte le risoluzioni e regolazioni della camera.
6. Rimozione USB: risparmio attivo e display in standby; reinserimento USB:
   ripristino automatico del profilo completo e del display.
7. Riavvio: foto precedente assente (404), come previsto per memoria volatile.
8. OTA cifrato e verifica dell'assorbimento reale a batteria.

La configurazione del caricatore non viene modificata, finché la batteria non
è identificata. Nessuna stima dell'autonomia è stata misurata.

## Aggiornamento 0.1.1

Analizzato Birdcam-logs.txt: abort xQueueSemaphoreTake durante setup e safe mode.
Corretto avvio anticipato del microfono tramite switch template. Validazione
dei tre casi e generazione C++ superate nuovamente con ESPHome 2026.9.1.
Il nuovo firmware resta da verificare sulla scheda; vedere CORREZIONE-0.1.1.md.
