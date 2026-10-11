# Prova registrazione WAV — versione 0.2.0

Questa versione registra audio per la futura integrazione BirdNET. Non identifica
ancora specie, non contatta un server e non invia registrazioni su Internet.

## Aggiornamento

Sostituisci **birdcam.yaml e tutta la cartella components/birdcam/**. Ci sono due
nuovi file C++: audio_recorder.h e audio_recorder.cpp. Fonts e secrets restano
quelli esistenti. Ricompila in ESPHome Device Builder e installa via OTA.

La configurazione deve essere aggiornata insieme al componente. Il microfono
passa da 16 a 48 kHz anche per la misura RMS quando abilitata; il WAV è PCM
mono a 16 bit, convertito attraverso il MicrophoneSource ufficiale ESPHome.

## Comandi in Home Assistant

- **Modalita BirdNET WAV**: Spento (a ogni avvio), Solo manuale, PIR, Audio,
  PIR e audio. Il microfono viene avviato quando la modalità è abilitata.
- **Registra WAV**: registra quando una modalità è abilitata. Attendi un paio
  di secondi dopo l'attivazione, poi premi. Il pulsante rispetta l'intervallo.
- **Durata registrazione WAV**: 3–10 secondi, predefinito 6. Una modifica vale
  per la prossima clip; non taglia la registrazione in corso.
- **Intervallo minimo registrazioni**: 15–3600 secondi, predefinito 60,
  misurato dall'avvio di una registrazione all'avvio della successiva.
- **Soglia audio / Durata minima audio**: regolano sia il trigger foto audio
  sia la registrazione nelle modalità Audio e PIR e audio.

Il cooldown WAV è distinto dall'intervallo minimo foto. Vale per PIR, audio e
pulsante manuale insieme; non viene azzerato cambiando modalità. Non si accodano
eventi: quelli arrivati durante registrazione o cooldown vengono ignorati.
Gli errori di memoria o di microfono applicano comunque il limite per non creare
tentativi continui. Puoi impostare 300 secondi per limitare gli avvii a uno ogni
cinque minuti anche in presenza di molti uccelli o rumore continuo.

Gli switch **Trigger PIR** e **Trigger audio** controllano le foto; il selettore
WAV controlla separatamente le registrazioni. Il trigger foto audio rimane spento
di default. Audio/PIR e audio nel selettore accendono comunque l'ascolto necessario
al trigger WAV, anche con lo switch foto audio spento. Spegnere il selettore
interrompe e scarta una clip incompleta. Se il trigger foto audio è acceso, il
microfono continua a servire quel trigger.

## Download e prima prova

1. Seleziona **Solo manuale**, attendi due secondi e premi **Registra WAV**.
2. Dopo circa sei secondi verifica Registrazioni WAV = 1 e Dimensione ultimo
   WAV = **576044 byte** (576000 audio + 44 intestazione).
3. Scarica `http://IP_BIRDCAM:8082/last.wav` e ascoltalo. Prima della prima
   clip completata l'endpoint risponde 404. Il WAV precedente resta disponibile
   durante una nuova registrazione e se questa fallisce.
4. Provalo nella [demo ufficiale BirdNET](https://birdnet.cornell.edu/demo/).
5. Attiva Audio oppure PIR e audio; regola soglia e intervallo osservando
   il rumore reale. Non c'è un buffer precedente al trigger: il suono che ha
   fatto partire la registrazione può essere già terminato all'inizio della clip.

La clip non è garantita sincronizzata con la foto: sono acquisizioni indipendenti.
La modalità WAV non attiva automaticamente gli switch foto disattivati.

## Memoria e carico

Una clip richiede `48000 × 2 × durata + 44` byte: 288044 a 3 secondi,
576044 a 6, 960044 a 10. Si alloca l'intero buffer in PSRAM prima di iniziare;
durante la registrazione arrivano solo copie di piccoli blocchi PCM. Non viene
creata una seconda copia completa per il download HTTP.

Durante una nuova clip restano in memoria quella precedente e quella in corso.
Un download della vecchia clip può trattenerla anche dopo una sostituzione:
considera fino a tre buffer WAV, oltre a JPEG, framebuffer e memoria ESPHome.
Il server HTTP condiviso serve il download a blocchi: un download lento può
ritardare le altre richieste sulla porta 8082, ma non usa il loop per inviare dati.

Controlla PSRAM libera, PSRAM blocco massimo, Heap libero/minimo, Errori WAV,
Ultimo errore WAV, Registrazione WAV in corso e Attesa prossima registrazione.
I contatori di perdita rilevano blocchi rifiutati dal registratore; non misurano
ogni possibile perdita a monte nel driver I2S o DMA.

Per la prima prova usa VGA a 5–10 FPS e apri lo stream mentre registri. Verifica
audio senza interruzioni, assenza di riavvii, memoria stabile dopo clip ripetute
e disponibilità dei JPEG durante il download WAV. Il livello RMS aggiorna solo
quando l'ascolto relativo è attivo; fuori ascolto può mostrare l'ultimo valore.

Con modalità Spento non si conserva un buffer di registrazione in corso; l'ultimo
WAV completato resta scaricabile. Foto e WAV non vengono scritti in flash e
si perdono al riavvio. Questa versione è sperimentale: il funzionamento a 48 kHz
con video attivo va confermato sulla tua scheda.
