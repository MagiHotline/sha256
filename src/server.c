#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <openssl/sha.h>
#include "common.h"

int active_processes = 0; // risorsa condivisa per contare i processi attivi
int max_processes = MAX_CONCURRENT;

// Code per le richieste
struct msg_request priority_queue[100];
struct msg_request fcfs_queue[100];

int priority_size = 0;
int fcfs_size = 0;

// Semaforo per proteggere contatore processi
int sem_id;

void sem_wait() {
    // (Decrementa il semaforo)
    struct sembuf op = {0, -1, 0};
    semop(sem_id, &op, 1);
}

void sem_signal() {
    // (Incrementa il semaforo)
    struct sembuf op = {0, +1, 0};
    semop(sem_id, &op, 1);
}

// Funzione per aggiungere una richiesta alla coda
void enqueue(struct msg_request req) {
    switch(req.scheduling_policy) {
        case SCHED_FCFS: {
            // FCFS - add to the end of the queue
            fcfs_queue[fcfs_size++] = req;
            break;
        }
        case SCHED_PRIORITY:   {
            // Priority - insert sorted by size
            // Insert and sort descending
            priority_queue[priority_size] = req;
            for (int i = priority_size; i > 0; i--) {
                if (priority_queue[i].size > priority_queue[i-1].size) {
                    struct msg_request tmp = priority_queue[i];
                    priority_queue[i] = priority_queue[i-1];
                    priority_queue[i-1] = tmp;
                } else break;
            }

            priority_size++;
            break;
        }
    }
}

// New dequeue function
struct msg_request dequeue() {
    // 1. Check priority queue
    if (priority_size > 0) {
        return priority_queue[--priority_size];
    }
    // 2. Check FCFS queue
    if (fcfs_size > 0) {
        return fcfs_queue[--fcfs_size];
    }
    // Fallback (shouldn't happen)
    struct msg_request empty = {0};
    return empty;
}

// Check if queue has requests
int has_queued_requests() {
    return (fcfs_size > 0) || (priority_size > 0);
}

// Processo per spawnare un worker
void spawn_worker(struct msg_request req) {
    pid_t pid = fork();
    if (pid == 0) {
        // figlio
        char shm_key_str[32];
        char client_pid_buf[32];
        char size_str[32];
        snprintf(shm_key_str, sizeof(shm_key_str), "%d", SHM_KEY);
        snprintf(client_pid_buf, sizeof(client_pid_buf), "%d", req.pid);
        snprintf(size_str, sizeof(size_str), "%zu", req.size);

        // Esegui il worker con filename e PID del client
        execl("./worker", "worker", shm_key_str, size_str, client_pid_buf, NULL);
        // Se execl fallisce
        perror("execl failed");
        exit(1);
    } else if (pid > 0) {
        // padre
        sem_wait();       // protezione accesso a active_processes
        active_processes++;
        sem_signal();
    } else {
        perror("fork failed");
    }
}


// Gestore del segnale SIGCHLD per gestire la terminazione dei processi figli
void sigchld_handler(int signum) {
    wait(NULL);
    sem_wait(); // Proteggiamo l'accesso alla risorsa condivisa
    active_processes--;
    sem_signal();
}

int main() {
    // SIGCHILD : to parent on child stop or exit
    signal(SIGCHLD, sigchld_handler);

    // Usiamo solo la coda dei messaggi delle richieste
    int msgid = msgget(REQ_MSG_KEY, IPC_CREAT | 0666);
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    int shmid = shmget(SHM_KEY, MAX_FILE_SIZE, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    semctl(sem_id, 0, SETVAL, 1);
    printf("Server avviato\n");

    while (1) {
        // Controlla se ci sono richieste in coda e controllo il tipo di richiesta
        struct msg_request req;

        if (msgrcv(msgid, &req, sizeof(req) - sizeof(long), 0, IPC_NOWAIT) != -1) {
            // Se il tipo di messaggio è una REQ_MTYPE, lo mettiamo in coda
            if (req.mtype == REQ_MTYPE) {
                enqueue(req);
            } else if (req.mtype == CTRL_MTYPE) {
                // Se invece è un messaggio di controllo, aggiorniamo il limite dei processi
                max_processes = req.size;
                printf("Nuovo limite processi: %d\n", max_processes);
            }
        }

        // se la coda non è vuota e possiamo spawnare un nuovo worker
        if (has_queued_requests()) {
            sem_wait(); // Proteggiamo l'accesso alla risorsa condivisa
            int can_spawn = (active_processes < max_processes);
            sem_signal();
            if (can_spawn) {
                struct msg_request next = dequeue();
                printf("Spawning worker per la richiesta di dimensione %zu da PID %d\n", next.size, next.pid);
                spawn_worker(next);
            }
        }
    }

    return 0;
}
