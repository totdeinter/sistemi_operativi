#!/bin/bash

uso(){
    echo "Uso: $0 <directory_assoluta> <carattere 1> [carattere 2]..."
    echo "Descrizione:"
    echo "<directory assoluta> : Nome assoluto di una dirctory"
    echo "<carattereN> : Caratteri singoli da ricercare nel contenuto del file"
    echo "Esempio: $0 /tmp/gerarchia a b c"
    exit 1;
}

#controllo numero dei parametri
[ "$#" -lt 2 ] && echo "Errore sono richiesti almeno 2 parametri (directory + almeno un carattere)" && uso

#controllo tipo dei param
#controllo primo param sia singola gerarchia
[ ! -d "$1" ] && echo "Errore: '$1' non è una directory valida" && exit 2

G=$1 #memorizzo la gerarchia
shift

#controllo tutti i caratteri passati come parametri siano singoli caratteri
for c; do
    [ ${#c} -ne 1 ] && echo "Errore '$c' non è un singolo carattere" && exit 3
done

#controlli sui parametri terminati
CARATTERI=("$@") #tutti i parametri (caratteri) li memorizzo nell'array

#dichiaro array associativo: nomeDirectory (chiave) -> 0/1 (valore)
#servirà per la stampa: 0 stampa con '-' davanti, 1 con '+' davanti
declare -A DIR_OK
#per contare i file trovati che rispettano le specifiche in tutta la gerarchia
CONT=0

#inizializzo l'array
while IFS= read -r -d '' D; do #legge anche nomi di dir con spazi o caratteri speciali
    DIR_OK["$D"]=0
done < <(find "$G" -type d -print0)

#analizzo tutti i file in una volta sola
while IFS= read -r -d '' F; do

    FILE_OK=0
    #controllo che il file contenga almeno un'occorrenza di almeno uno dei caratteri
    for c in "${CARATTERI[@]}"; do
        grep -Fq "$c" "$F" && { FILE_OK=1; break; }
    done

    #se ho trovato almeno un file
    if [ "$FILE_OK" -eq 1 ]; then
        ((CONT++))
        d=$(dirname "$F")
        DIR_OK["$d"]=1 #registro che in quella directory è stato trovato un file
    fi
done < <(find "$G" -type f -readable -print0)

#stampa nell'ordine corretto
for D in "${!DIR_OK[@]}"; do
    if [ "${DIR_OK[$D]}" -eq 1 ]; then
        echo "+ $D" #stampiamo col + davanti le cartelle con almeno 1 file
    else
        echo "- $D"
    fi
done

#stampa i file totali trovati
echo "Total files: $CONT-1"