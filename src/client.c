#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

int main(int argc, char *argv[]) {

    // Prendi algoritmo e file come argomento del client
    if (argc < 3) {
        printf("Uso: %s <algorithm> <file>\n", argv[0]);
        printf("Algorithms: 0=FCFS, 1=Priority\n");
        return 1;
    }

    int scheduling_policy = atoi(argv[1]);
    // Vedi se la scheduling_policy è valida
    if (scheduling_policy != SCHED_FCFS && scheduling_policy != SCHED_PRIORITY) {
        printf("Invalid scheduling policy. Use 0 for FCFS or 1 for Priority.\n");
        return 1;
    } else {
        printf("Scheduling policy: %s\n", scheduling_policy == SCHED_FCFS ? "FCFS" : "Priority");
    }

    char *filename = argv[2];
    printf("Filename: %s\n", filename);
    // Apri il file
    int file = open(filename, O_RDONLY, 0);
    printf("Opening file %s\n", filename);
    if (file == -1) {
        printf("File %s does not exist\n", filename);
        exit(1);
    }

    // Leggi il file in un buffer
    char buffer[MAX_FILE_SIZE];
    ssize_t bytes = read(file, buffer, 32);
    close(file);

    // Otteniamo l'ID della memoria condivisa
    int shmid = shmget(SHM_KEY, bytes, IPC_CREAT | 0666);
    char *shared_data = shmat(shmid, NULL, 0);
    memcpy(shared_data, buffer, bytes); // Copia i dati dal buffer alla memoria condivisa
    shmdt(shared_data); // "Stacca" la memoria condivisa

    // Ottieni la coda dei messaggi per le richieste
    int req_msqid = msgget(REQ_MSG_KEY, 0666);
    if (req_msqid == -1) {
        perror("msgget");
        return 1;
    }

    // Prepariamo la struttura del messaggio di richiesta
    struct msg_request req;
    req.mtype = REQ_MTYPE;
    req.size = bytes;
    req.pid = getpid();
    req.scheduling_policy = scheduling_policy;

    // Invia la richiesta al server
    printf("Sending request for file with size: %zu, PID: %d\n", req.size, req.pid);
    if (msgsnd(req_msqid, &req, sizeof(req) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        return 1;
    }
    printf("[CLIENT] Successfully sent request!\n");

    // Prendiamo la coda dei messaggi per le risposte
    int resp_msqid = msgget(RESP_MSG_KEY, 0666 | IPC_CREAT); // Client creates response queue
    if (resp_msqid == -1) {
        perror("msgget response queue");
        return 1;
    }

    // Attendi risposta
    printf("Waiting for response...\n");
    printf("Client waiting on mtype = %d\n", getpid());
    printf("Message queue ID: %d\n", resp_msqid);
    struct msg_response resp;
    if (msgrcv(resp_msqid, &resp, sizeof(resp) - sizeof(long), getpid(), 0) == -1) {
        perror("msgrcv");
        return 1;
    }

    printf("[CLIENT] Received response!\n");
    printf("SHA-256: %s\n", resp.hash);
    return 0;
}
