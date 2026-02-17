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
    char c_ass; //carattere i-esimo associato al figlio i-esimo
    pid_t pid; //pid del figlio
    long occ; //numero occorrenze del carattere trovate
} Risultato;

typedef int pipe_t[2]; //lati scrittura/lettura per ogni singola pipe

//calcola il numero di occorrenze di un carattere in un file
int calcolaOcc(const char *filename, const char car){
    //apro il file associato
    int fd = open(filename, O_RDONLY);
    char c = ' ';
    int occ = 0;
    if(fd < 0){ //errore apertura file
        fprintf(stderr, "Figlio %d: Errore nell'apertura del file %s: %s\n", getpid(), filename, strerror(errno));
        close(fd);
        return -1;
    }

    //calcoliamo il numero di occorrenze del carattere
    while(read(fd, &c, 1) > 0){
        if(c == car) occ++;
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
            exit(255);
        }
    }

    //abbiamo contato le occorrenze di tutti i caratteri della stringa s
    //ora dobbiamo scrivere su pipe
    size_t dim = L * sizeof(int);
    if(write(write_fd, v, sizeof(dim)) != sizeof(dim)){ //errore scrittura su pipe
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
    //controllo nome del file non sia vuoto
    if(strlen(argv[1]) == 0){
        fprintf(stderr, "Errore: la stringa '%s' è vuota\n", argv[1]);
        return EXIT_FAILURE;
    }

    //controllo nomi di file
    const char *stringa = argv[1];
    int L = strlen(argv[1]); // lunghezza dei caratteri
    int N = argc-2; //num processi figli da creare

    //controllo nomi di file non siano vuoti
    for(int i = 2; i < argc; i++){
        if(strlen(argv[1]) == 0){
            fprintf(stderr, "Errore: Il parametro in pos '%d' è vuoto '%s'\n", i, argv[i]);
            return EXIT_FAILURE;
        }
    }
    //controllo param completato

    //creazione array per i pid dei figli
    pid_t *pidFigli = (pid_t *) malloc(N * sizeof(pid_t));
    if(pidFigli == NULL){
        perror("Errore allocazione memoria pidFigli");
        return EXIT_FAILURE;
    }

    //creazione array di pipes
    pipe_t *pipes = malloc(N * sizeof(pipe_t));
    if(pipes == NULL){
        perror("Errore allocazione memoria pipes");
        free(pidFigli);
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
            free(pidFigli);
            free(pipes);
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
            free(pidFigli);
            free(pipes);
            return EXIT_FAILURE;
        }

        if(pid == 0){ //codice figlio
            //ogni figlio scrive su pipes[i]
            //per ogni figlio chiudo il lato di lettura pipes[i][0]
            //per ogni figlio lascio aperto solo il lato di scrittura pipes[i][1]

            //chiusura pipe inutilizzate
            //close(pipes[i][0]);
            for(int j = 0; j <N; j++){ //chiusura lati scrittura altri figli
                close(pipes[j][0]);
                if(j != i) close(pipes[j][1]);
            }

            const char *filename = argv[i + 2];

            codiceFiglio(filename, stringa, L, pipes[i][1]);

        }else{ //sono nel padre
            //salviamo il pid del figlio
            pidFigli[i] = pid;
            //chiusura pipe inutilizzate
            close(pipes[i][1]); //il padre non scrive mai
        }
    }

    //codice del padre
    int errore = 0;

    //creiamo la struttura che il padre leggerà dai figli
    int *arr_letto_da_pipe = (int *) malloc(N * sizeof(int));

    for(int i = 0; i < N; i++){

        //leggo dalla pipe, controllando il numero di byte letti
        //mi aspetto di leggere un array di L interi
        ssize_t n = read(pipes[i][0], arr_letto_da_pipe, sizeof(L * sizeof(int)));
        if(n != sizeof(L * sizeof(int))){ //errore nella lettura
            if (n >= 0){ //errore di comunicazione
                errore = -1;
            }else{ //errore di sistema
                perror("Errore lettura da pipe");
                errore = -2;
            }
        }
        //chiudo il lato di lettura della pipe
        close(pipes[i][0]);

        //aspettiamo i figli
        int status;
        if(waitpid(pidFigli[i], &status, 0) == -1){
            perror("Errore in waitpid");
        }else if(!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS || errore != 0){
            //Terminazione anomala del figlio
            if(errore != -1) errore=-3;
        }
    }

    //stampa dei risultati
    for(int i = 0; i < N; i++){
        if(errore != 0){//errore
            printf("%s %d Error\n", argv[i + 2], pidFigli[i]);
            //break;
        }else{
            printf("%s %d ", argv[i + 2], pidFigli[i]);
            for(int j = 0; j < L; j++){ //stampo i caratteri
                printf("%c: %d ", stringa[j], arr_letto_da_pipe[j]);
            }
            printf("\n");
        }
    }

    free(arr_letto_da_pipe); //libero la memoria
    free(pipes);
    free(pidFigli);
    return errore == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}