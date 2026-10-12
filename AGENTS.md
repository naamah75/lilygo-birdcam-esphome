# Istruzioni per gli agenti

## Progetto condiviso tra due PC

Questo progetto viene modificato con Codex da **due PC distinti**. Le copie
locali possono essere diverse: GitHub (`origin`) e la cronologia Git sono il
punto di scambio delle modifiche. Non presumere che la copia locale sia aggiornata.

### Prima di iniziare ogni sessione

1. Controllare `git status --short --branch`, il branch corrente e il suo upstream.
2. Eseguire `git fetch origin` e confrontare il branch locale con quello remoto.
3. Se il working tree è pulito e il branch è soltanto indietro, aggiornarlo con
   `git pull --ff-only`. Se è già aggiornato, proseguire.
4. Se esistono modifiche non committate, preservarle e verificarne il contenuto
   prima di integrare aggiornamenti remoti. Non eseguire stash o ripristini
   automatici che nascondano o cancellino il lavoro dell'utente.
5. Se i branch sono divergenti, esaminare entrambe le serie di commit e integrare
   le modifiche con un merge; usare rebase solo per commit locali non pubblicati
   quando appropriato. Se un conflitto richiede una scelta funzionale ambigua,
   chiedere all'utente conservando entrambe le versioni.
6. Se GitHub non è raggiungibile, segnalare che la sincronizzazione non è stata
   verificata. È possibile preparare modifiche locali, ma non dichiarare il
   repository sincronizzato.

### Durante il lavoro

- Preservare modifiche e file creati sull'altro PC, anche se estranei al compito.
- Non usare `git reset --hard`, `git clean`, checkout distruttivi o push forzati
  per risolvere differenze tra le copie locali.
- Non inserire percorsi assoluti specifici di un PC nella configurazione condivisa.
- Non committare credenziali, `secrets.yaml` o artefatti di compilazione locali.
- Mantenere modifiche coerenti con README.md e documentare in VALIDAZIONE.md
  i controlli effettivamente eseguiti quando cambia il firmware.

### Prima di pubblicare e alla fine della sessione

1. Verificare il diff e svolgere i controlli pertinenti alle modifiche.
2. Creare commit mirati includendo soltanto i file del compito.
3. Prima del push, eseguire nuovamente `git fetch origin`: l'altro PC potrebbe
   aver pubblicato nuovi commit durante la sessione. Integrare gli aggiornamenti
   e ripetere i controlli interessati prima di pubblicare.
4. Usare un push normale. Se viene rifiutato perché il remoto è avanzato,
   ripetere fetch e integrazione senza forzare il push.
5. Riferire branch, commit, esito del push ed eventuali modifiche rimaste locali.
   Non dichiarare disponibili sull'altro PC modifiche non pubblicate su GitHub.

Sull'altro PC applicare lo stesso controllo e aggiornamento prima di riprendere
il lavoro. GitHub sincronizza i file versionati; cronologia delle chat Codex,
segreti e impostazioni locali devono essere gestiti separatamente.
