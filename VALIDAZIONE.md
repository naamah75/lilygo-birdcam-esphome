# Verifica 0.3.0 RTSP audio

Target: LILYGO T-Camera S3 V1.6 I2S, ESPHome 2026.9.1 / ESP-IDF 5.5.5.

## Completato

- Validazione YAML completa, porta foto fuori intervallo e camera inesistente.
- Compilazione firmware completa, link e generazione OTA/factory/ELF riusciti:
  [Actions #4](https://github.com/naamah75/lilygo-birdcam-esphome/actions/runs/38205808632),
  commit 998afac73eb1c492c72ba4192c75a9c746c9110a.
- Test del codice reale rtsp_audio.cpp su Linux con API ESPHome, PMU e microfono
  simulate, socket POSIX reali: off default, policy USB, microfono su PLAY,
  richieste frammentate, DESCRIBE/SDP, rifiuto UDP, sessione, singolo client,
  PCM L16 big-endian, numerazione/timestamp RTP, report RTCP, keepalive,
  TEARDOWN, spegnimento, overflow e riconnessione.
- Decodifica reale tramite FFmpeg RTSP/TCP: PCM ricevuto uguale ai campioni
  simulati; step test riuscito in
  [Actions #5](https://github.com/naamah75/lilygo-birdcam-esphome/actions/runs/38205921701),
  commit 8203a5b384812963961d1a0dd292b9ed37c912a7.
  Il firmware è identico ad Actions #4; questo commit estende test/workflow.

RAM statica: 129207 / 341760 byte (37,8%). Flash applicazione:
1059195 / 8126464 byte (13,0%). Non sono misure di allocazioni dinamiche,
CPU, PSRAM durante video o autonomia. Il nuovo buffer audio PSRAM è di
48000 byte, oltre a stack task 8192 byte e buffer di driver/microfono/rete.

La CI usa secrets dimostrativi e chiave API casuale. Nessun binario CI viene
consegnato: compilare in Device Builder con i propri secrets.yaml.

## Da verificare sulla scheda

Il precedente WAV 48 kHz dell'utente conteneva audio utile a livello basso;
RTSP applica guadagno 32x alla propria MicrophoneSource. Non è una prova di
RTSP su ESP32. Restano qualità audio, saturazione, driver I2S/DMA, camera e
foto simultanee, USB/batteria, riconnessione Wi-Fi e compatibilità end-to-end
con il server BirdNET-Go 20261010 dell'utente. Nessun canto è stato analizzato
da questa CI: FFmpeg riceve campioni sintetici, non un modello BirdNET.

Il test host non simula lo scheduling ESP32 o il consumo energetico.
Vedere PROVA-RTSP.md per installazione e prova.

## Ambiente locale

La validazione locale è riuscita. La generazione C++ è iniziata ma il download
noise-c dal registry PlatformIO è fallito per risoluzione DNS. La compilazione
completa è stata quindi verificata su Linux/GitHub Actions.
