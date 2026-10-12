# Prova RTSP audio — 0.3.0 sperimentale

Il server RTSP è sulla Birdcam. Non occorrono MediaMTX, proxy o nuovi servizi
Linux. Il flusso contiene esclusivamente audio PCM L16 mono, 48 kHz, 16 bit.
Video e ultima foto continuano a usare gli indirizzi HTTP esistenti.

## Installazione in ESPHome

1. Sostituisci birdcam.yaml.
2. Sostituisci tutta la cartella components/birdcam/ con quella del pacchetto:
   __init__.py, birdcam.h, birdcam.cpp, rtsp_audio.h, rtsp_audio.cpp, rtsp_protocol.h.
   Rimuovi audio_recorder.h e audio_recorder.cpp della versione precedente.
   ESPHome compila tutti i sorgenti del componente: non lasciare i vecchi file.
3. Mantieni i tuoi secrets.yaml e fonts/.
4. Device Builder: Validate, poi Install / Wirelessly.
5. Le vecchie entità WAV possono restare come non disponibili nel registro HA;
   puoi eliminarle dal registro entità. Non sono più pubblicate dal firmware.

## Home Assistant

**Streaming audio BirdNET**: Spento a ogni avvio, Solo USB, Sempre.
Solo USB richiede alimentazione USB rilevata dalla PMU; con lettura non valida
lo streaming è disabilitato. Sempre abilita anche su batteria.
Il microfono RTSP parte su PLAY e si ferma alla disconnessione, salvo l'altro
consumatore indipendente, Trigger audio per le foto. Quest'ultimo resta spento
all'avvio; accenderlo avvia il proprio misuratore RMS senza abilitare RTSP.

**URL RTSP audio** contiene l'indirizzo da copiare in BirdNET-Go:

    rtsp://IP_BIRDCAM:8554/audio

Le entità RTSP abilitato, Client RTSP connesso e Streaming audio attivo distinguono
abilitazione, connessione e PLAY richiesto dal client. PLAY non certifica che
il microfono produca audio: controlla anche Pacchetti RTP in crescita.
Pacchetti RTP, Connessioni RTSP, Errori RTSP, Blocchi audio persi RTSP,
Campioni audio saturati RTSP e Buffer audio RTSP sono diagnostici cumulativi
fino al riavvio. Le perdite contate sono rifiuti del buffer/callback del server,
non tutte le eventuali perdite del driver I2S/DMA.

## BirdNET-Go 20261010

Nel pannello impostazioni audio di BirdNET-Go aggiungi una sorgente RTSP con
l'URL sopra e scegli **TCP** come trasporto. Se è presente la scelta del tipo
media, scegli solo audio. Il server Birdcam rifiuta UDP con codice RTSP 461.
Non usare last.wav come sorgente: è stato rimosso. Non eseguire il comando file,
assente nel binario 20261010 dell'utente.

Per la prima prova alimenta via USB, imposta Solo USB e usa video VGA a 5 FPS.
Controlla connessione, pacchetti in crescita e livello audio in BirdNET-Go.
Apri poi il video e prova foto PIR. Se aumentano Errori RTSP o Blocchi audio
persi RTSP, riduci FPS/risoluzione del video e verifica RSSI e copertura Wi-Fi.
Imposta posizione geografica e soglia di confidenza nel server. BirdNET-Go
ascolta continuamente: i limiti per rilevazioni ripetute vanno impostati lì.
Il cooldown delle foto continua a funzionare sulla scheda.

Se VLC è già disponibile, si può ascoltare l'URL con trasporto RTSP/TCP.
Un solo client è consentito: spegni/disabilita la sorgente BirdNET-Go durante
una prova VLC, poi chiudi VLC e riattiva la sorgente. Il listener viene chiuso
mentre una sessione è attiva; un secondo client non viene accodato.

## Memoria, volume e risparmio

Buffer circolare PSRAM: 48000 byte, circa 0,5 secondi. Nessuna registrazione
WAV né memorizzazione di audio a lungo termine. Task RTSP: stack 8192 byte,
oltre alle allocazioni di ESPHome/microfono/socket. Banda PCM: 768 kbit/s,
circa 0,8 Mbit/s con RTP/TCP/IP, più il video.

Guadagno del solo flusso RTSP: gain_factor: 32 nel blocco birdcam.microphone.
Puoi regolarlo a 1–64 nel YAML e ricompilare. Il trigger foto e il sensore RMS
mantengono il loro guadagno originale, evitando di cambiare la soglia foto.
Audio RMS e Audio picco si aggiornano solo con Trigger audio acceso; RTSP ha
un consumatore indipendente e non accende il misuratore fotografico.
Campioni saturati in crescita indicano possibili distorsioni: riduci il guadagno.

Se manca audio per 8 secondi, manca controllo RTSP per 65 secondi, un invio
resta bloccato per 300 ms o il buffer perde un blocco, la sessione viene chiusa.
BirdNET-Go deve riconnettersi; il firmware non riproduce vecchio audio per
recuperare una rete troppo lenta. Spegnere il selettore o rimuovere USB in Solo
USB interrompe la sessione. Reinserire USB riabilita il server per la riconnessione.

RTSP è locale e senza autenticazione, come gli endpoint HTTP già presenti;
non inoltrare la porta su Internet. L'API HA resta cifrata.

## Prova hardware

Questa versione deve essere verificata sulla scheda con BirdNET-Go 20261010:
qualità audio, video simultaneo, foto PIR, spegnimento/riaccensione selettore,
USB/batteria, memoria stabile e riconnessione dopo perdita Wi-Fi. In particolare
la compilazione e i test host non misurano carico CPU, consumi o perdite I2S.
