# Birdcam — LILYGO T-Camera S3 V1.6 I²S

Firmware ESPHome con trigger PIR/audio locali e ultima foto esclusivamente in PSRAM.
Versione di riferimento: **ESPHome 2026.9.1**, framework **ESP-IDF**, flash 16 MB,
PSRAM octal 8 MB. Il firmware usa la flash per il programma; non vi scrive JPEG.
Impostazioni e contatori non vengono ripristinati dalla flash e ripartono al riavvio.

## Installazione in Home Assistant

1. Copia `birdcam.yaml`, `components/` e `fonts/` nella directory di configurazione
   di ESPHome Device Builder, normalmente `/config/esphome/`. Mantieni la struttura:

   ```text
   esphome/
   ├── birdcam.yaml
   ├── secrets.yaml
   ├── components/birdcam/__init__.py
   ├── components/birdcam/birdcam.h
   ├── components/birdcam/birdcam.cpp
   └── fonts/DejaVuSansMono.ttf
   ```

2. Aggiungi le chiavi di `secrets.yaml.example` al tuo `secrets.yaml` esistente.
   Sostituisci tutti gli esempi, compresa la chiave API: la chiave a zeri è solo
   un segnaposto, non una chiave sicura. Per crearne una: crea temporaneamente un
   dispositivo nel wizard di Device Builder e copia il valore generato in
   `api.encryption.key`, oppure genera 32 byte casuali e codificali base64.
3. Premi **Validate**, poi **Install**. Anche gli aggiornamenti OTA sono cifrati,
   usando la stessa chiave API. La prima installazione è via USB; usa il
   download del firmware e il flasher web se il dispositivo è collegato al tuo PC
   anziché alla macchina che ospita HA. In seguito usa OTA mentre è raggiungibile.
4. In HA aggiungi il dispositivo scoperto dall'integrazione ESPHome e inserisci
   la chiave API. Camera live, sensori, pulsanti e controlli sono nativi ESPHome.
5. Premi **Scatta foto**, dopo almeno 2 secondi dall'avvio della camera, e verifica
   `http://IP_BIRDCAM:8082/last.jpg`.
6. Per mostrare l'ultima foto aggiungi una **Generic Camera** in HA con URL immagine
   `http://IP_BIRDCAM:8082/last.jpg`, senza URL stream. ESPHome 2026.9.1 usa una
   camera singleton per l'API: questa seconda vista richiede configurazione manuale.
   Lascia disattivato "Limit refetch to URL change", perché il percorso rimane
   uguale e cambia il contenuto. La risposta è 404 finché non è stata acquisita una foto in questo avvio.
   Assegna un IP stabile tramite prenotazione DHCP.

## Comportamento

Il trigger audio è **disattivato a ogni avvio**; attivalo dallo switch in HA.
Il PIR è attivo di default. Tutte le 57 entità ESPHome hanno un'icona dedicata.

- PIR o rumore persistente sopra soglia richiedono uno scatto direttamente sulla
  scheda; Home Assistant non esegue l'automazione.
- Due fotogrammi vengono scartati prima della copia JPEG per evitare il vecchio
  buffer in coda. Il ritardo cresce a FPS bassi e va misurato con gli uccelli reali.
- L'ultima foto viene sostituita soltanto dopo una copia riuscita. Se manca PSRAM,
  il JPEG supera 1 MiB o la cattura scade, resta disponibile la foto precedente.
- La risposta HTTP mantiene la propria copia condivisa anche quando arriva un
  nuovo evento; non usa il buffer della camera e non richiede un nuovo scatto.
- Il cooldown vale per tutti i trigger, incluso il pulsante manuale. Un segnale
  PIR sempre alto produce un evento sul fronte di salita, non scatti ripetuti.
- La fotocamera e i trigger continuano a funzionare senza HA o Wi-Fi. I timeout di
  reboot API/Wi-Fi sono disabilitati per non perdere inutilmente la foto in RAM.
- Riavvio, spegnimento e perdita di alimentazione cancellano JPEG e contatori.
  **Non è implementato deep sleep**, per preservare l'ultima foto in PSRAM.

## USB e batteria

La presenza USB è letta dall'AXP2101 ogni secondo. Su USB vengono applicati FPS USB,
Wi-Fi senza modem power save e display acceso. Il toggle "Risparmio a batteria"
non applica limitazioni quando USB è presente.

A batteria, con risparmio attivo: FPS batteria, richieste idle disattivate,
Wi-Fi modem power save e display in standby dopo il timeout. Il display viene
fermato anche negli aggiornamenti. Camera e microfono restano alimentati:
questo è un risparmio moderato, non una modalità da fototrappola a lunga autonomia.
Gli FPS sono limiti richiesti, non misure del frame rate effettivo.
Per ridurre il consumo audio disattiva "Trigger audio": ferma l'ascolto.

Il firmware non modifica corrente di carica, tensione finale o protezione TS
del caricatore, perché capacità e caratteristiche della batteria non sono note.
La carica segue quindi la configurazione già presente nel PMU: se non funziona,
occorre prima identificare la batteria e impostare il caricatore consapevolmente.
Non viene promessa una durata in ore senza una misura sull'hardware.
"Batteria PMU stima" è il valore del fuel gauge, non una misura calibrata da noi.

## Regolazioni camera e audio

- Risoluzioni: QVGA, VGA, SVGA, XGA, SXGA, UXGA. I buffer sono inizializzati alla
  dimensione massima per consentire l'aumento della risoluzione durante l'uso.
- Luminosità, contrasto, saturazione, qualità JPEG, esposizione automatica/manuale,
  compensazione esposizione, guadagno automatico, specchio, flip e bianco.
- Qualità JPEG: un numero più basso corrisponde a qualità migliore e file maggiori.
- Durante uno scatto i cambi camera sono rifiutati: riprova quando termina.
  Dopo il cambio risoluzione può apparire un fotogramma transitorio nello stream.
- Livello audio in **dBFS** (negativo, relativo al fondo scala digitale), non dB SPL.
  Soglia iniziale -25 dBFS: regolala osservando il rumore di fondo e il suono desiderato.
  Il firmware rileva il livello, non riconosce specie o canti.
- Il trigger usa misure RMS ogni 100 ms; HA riceve il livello ogni 2 secondi.
  La durata minima viene valutata in finestre da 100 ms, con una finestra iniziale
  di rilevazione; non è un timestamp audio preciso.
- Se il livello audio è assente, verifica prima la V1.6 e prova `channel: right`
  al posto di `left` nel microfono. Verifica poi alimentazione e cablaggio.
- Nessuna impostazione è persistente: dopo reboot tornano i valori iniziali del YAML.

## Display e diagnostica

OLED: USB/BAT, tensione, RSSI, risoluzione, ECO/FULL, contatore e trigger, audio e ora.
Il fuso orario iniziale è Europe/Rome, modificabile nel YAML. Ora via SNTP;
la cattura funziona anche senza sincronizzazione. Su batteria il display si
spegne dopo il timeout; su USB si riaccende automaticamente.

HA: RSSI, qualità Wi-Fi derivata, uptime, IP/SSID/MAC, batteria/VBUS/VSYS,
ricarica, stima batteria, heap libero/minimo, PSRAM libera/blocco massimo,
contatore, errori, dimensione JPEG, tempo acquisizione ed età foto.
Motivo reset è il codice numerico `esp_reset_reason_t` (1 power-on, 3 software,
4 panic, 5 interrupt watchdog, 6 task watchdog, 9 brownout).

## Endpoint e accesso

- `http://IP:8080/`: live MJPEG (un client stream del server).
- `http://IP:8081/`: snapshot corrente, differente dalla foto dell'evento.
- `http://IP:8082/last.jpg`: ultima foto PIR/audio/manuale, senza cache.

Gli endpoint HTTP sono locali e senza autenticazione; l'API ESPHome è cifrata.
Usali nella LAN e accedi da remoto attraverso HA/VPN, senza inoltrare queste porte
su Internet. Due connessioni contemporanee massime per il server della foto.

## Compatibilità e verifiche

Il componente include validazione Python e code generation attuali; non usa
`custom_component`, vecchi `SENSOR_SCHEMA`, override dei componenti ufficiali
o librerie Arduino. La dipendenza IDF `esp_http_server` è esplicita.
Le API camera usate sono quelle della release 2026.9.1. `min_version` non blocca
gli aggiornamenti futuri: dopo aggiornamenti verifica nuovamente la compilazione.

Consulta `VALIDAZIONE.md` per i controlli effettivamente completati. La prova su
scheda resta necessaria: video e audio insieme, USB/batteria, ripresa OTA, passaggio
di risoluzione e scatti ripetuti con download simultanei.

Font DejaVu ridistribuito con la licenza in `fonts/LICENSE_DEJAVU`.
Riferimenti: https://github.com/Xinyuan-LilyGO/LilyGo-Cam-ESP32S3 e
https://esphome.io/components/external_components/.
