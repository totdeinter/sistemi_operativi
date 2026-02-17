#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

//struct per salvare i campi da stampare
typedef struct{
    int* occ_c; //array di interi con le occorrenze di ogni carattere trovate nel file i-esimo
    pid_t pid; //pid del figlio
} Risultato;

typedef int pipe_t[2]; //lati scrittura/lettura per ogni singola pipe

//calcola il numero di occorrenze di un carattere in un file
int calcolaOcc(const char *filename, const char car){
    //apro il file associato
    int fd = open(filename, O_RDONLY);
    char cLetto;
    int occ = 0;
    if(fd < 0){ //errore apertura file
        fprintf(stderr, "Figlio %d: Errore nell'apertura del file %s: %s\n", getpid(), filename, strerror(errno));
        close(fd);
        return -1; //ritorniamo -1 in caso di errore
    }

    //calcoliamo il numero di occorrenze del carattere
    while(read(fd, &cLetto, 1) > 0){
        if(cLetto == car) occ++;
    }
    close(fd);

    return occ;
}

//ogni figlio conta le occorrenze di ogni carattere nel proprio file associato
void codiceFiglio(const char *filename, const char *s, const int L, int write_fd){
    //creo array di int che poi dovrò inviare al padre
    int *v = (int *) malloc(L * sizeof(int));
    if(v == NULL){ //errore creazione array di int
        perror("Errore allocazione memoria nel figlio");
        close(write_fd);
        exit(255);
    }

    for(int i = 0; i < L; i++){
        v[i] = calcolaOcc(filename, s[i]);
        if(v[i] == -1){
            //errore lettura da file associato del carattere i
            close(write_fd);
            exit(255);
        }
    }

    //abbiamo contato le occorrenze di tutti i caratteri della stringa s
    //ora dobbiamo scrivere su pipe
    size_t dim = L * sizeof(int);
    if(write(write_fd, v, dim) != dim){ //errore scrittura su pipe
        fprintf(stderr, "Errore: figlio con pid %d non è riuscito a scrivere su pipe: %s\n", getpid(), strerror(errno));
        close(write_fd);
        exit(255);
    }
    //chiudo il lato di scrittura
    close(write_fd);

    exit(EXIT_SUCCESS);
}


int main(int argc, char **argv){
    //controllo numero dei parametri (almeno 2: una stringa e 1 nome di file)
    if(argc < 3){
        fprintf(stderr, "Uso: %s <stringa> <filename 1> [filename 2] ...\n", argv[0]);
        return EXIT_FAILURE;
    }

    //controllo tipo dei parametri
    //controllo la stringa passata come primo parametro non sia vuota
    if(strlen(argv[1]) == 0){
        fprintf(stderr, "Errore: la stringa '%s' è vuota\n", argv[1]);
        return EXIT_FAILURE;
    }

    //controllo nomi di file
    const char *stringa = argv[1];
    int L = strlen(argv[1]); //numero di caratteri di cui si dovraanno contare le occorrenze
    int N = argc-2; //num processi figli da creare

    //controllo nomi di file non siano vuoti
    for(int i = 2; i < argc; i++){
        if(strlen(argv[i]) == 0){
            fprintf(stderr, "Errore: Il parametro in pos '%d' è vuoto '%s'\n", i, argv[i]);
            return EXIT_FAILURE;
        }
    }
    //controllo param completato

    //creazione array di struct Risultato
    Risultato* ris = calloc(N, sizeof(Risultato));
    if(ris == NULL){
        perror("Errore allocazione memoria pipes");
        return EXIT_FAILURE;
    }

    //creazione array di pipes
    pipe_t *pipes = calloc(N, sizeof(pipe_t));
    if(pipes == NULL){
        perror("Errore allocazione memoria pipes");
        free(ris);
        return EXIT_FAILURE;
    }

    //generazione N pipes e creazione N figli
    for(int i = 0; i < N; i++){
        //creazione pipe
        if(pipe(pipes[i]) == -1){ //errore nella creazione pipe
            perror("Errore creazione pipe");
            //chiudo tutte le pipe create fino a questo punto
            for(int k = 0; k < i; k++){
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            free(pipes);
            free(ris);
            return EXIT_FAILURE;
        }

        //generazione figli
        pid_t pid = fork();
        if(pid == -1){ //errore creazione figli
            perror("Errore creazione figli");
            close(pipes[i][0]); //chiusura le pipes figlio i
            close(pipes[i][1]);
            //chiudiamo tutte le pipe in lettura dei figli precedenti
            for(int k = 0; k < i; k++)
                close(pipes[k][0]); //lasciamo aperto in scrittura
            free(pipes);
            free(ris);
            return EXIT_FAILURE;
        }

        if(pid == 0){ //codice figlio
            //ogni figlio scrive su pipes[i]
            //per ogni figlio chiudo il lato di lettura pipes[i][0]
            //per ogni figlio lascio aperto solo il lato di scrittura pipes[i][1]

            //chiusura pipe inutilizzate
            for(int j = 0; j <N; j++){
                close(pipes[j][0]);
                if(j != i) close(pipes[j][1]); //chiusura lati scrittura altri figli
            }

            const char *filename = argv[i + 2];

            codiceFiglio(filename, stringa, L, pipes[i][1]);

        }else{ //sono nel padre
            //salviamo il pid del figlio
            ris[i].pid=pid;
            //chiusura pipe inutilizzate
            close(pipes[i][1]); //il padre non scrive mai
        }
    }

    //codice del padre
    int errore = 0; //errore a livello globale nel programma

    //array che contiene il codice di errore per ogni figlio (se 0 ok, altrimenti errore)
    int* err = calloc(N, sizeof(int));

    //creiamo la struttura che il padre leggerà dai figli
    int *arr_letto_da_pipe = (int *) malloc(L * sizeof(int));

    for(int i = 0; i < N; i++){

        //leggo dalla pipe, controllando il numero di byte letti
        //mi aspetto di leggere un array di L interi
        ssize_t n = read(pipes[i][0], arr_letto_da_pipe, L * sizeof(int));
        if(n != L * sizeof(int)){ //errore nella lettura
            if (n >= 0){ //errore di comunicazione
                err[i] = -1;
            }else{ //errore di sistema
                perror("Errore lettura da pipe");
                err[i] = -2;
            }
        }else{
            //ris[i].occ_c = arr_letto_da_pipe; NO!!
            ris[i].occ_c = calloc(L, sizeof(int));
            memcpy(ris[i].occ_c, arr_letto_da_pipe, L * sizeof(int));
        }
        //chiudo il lato di lettura della pipe
        close(pipes[i][0]);

        //aspettiamo i figli
        int status;
        if(waitpid(ris[i].pid, &status, 0) == -1){
            perror("Errore in waitpid");
        }else if(!WIFEXITED(status) || WEXITSTATUS(status) == 255 || err[i] != 0){
            //Terminazione anomala del figlio
            if(err[i] != -1) err[i]=-3;
            errore = 1;
        }
    }

    //stampa dei risultati
    for(int i = 0; i < N; i++){
        if(err[i] != 0){//errore nel figlio i-esimo
            printf("%s %d Error\n", argv[i + 2], ris[i].pid);
            errore=1; //segnaliamo c'e' stato un errore nel programma (in uno dei figli)
            //break;
        }else{
            printf("%s %d ", argv[i + 2], ris[i].pid);
            for(int j = 0; j < L; j++){ //stampo i caratteri
                if(j == L-1) printf("%c: %d", stringa[j], ris[i].occ_c[j]);
                else printf("%c: %d, ", stringa[j], ris[i].occ_c[j]);
            }
            printf("\n");
        }
    }

    free(arr_letto_da_pipe); //libero la memoria
    for(int i = 0; i < N; i++)
        free(ris[i].occ_c);
    free(ris);
    free(pipes);
    free(err);

    return errore == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}