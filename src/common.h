#ifndef COMMON_H
#define COMMON_H

#include <unistd.h>
#include <stddef.h>

#define REQ_MSG_KEY 0x5678  // Per richieste client->server
#define RESP_MSG_KEY 0x5679 // Per risposte server->client
#define SEM_KEY 0x1111 // chiave per il semaforo
#define SHM_KEY 0x1234 // chiave per la memoria condivisa
#define MAX_FILE_SIZE 4096 // massima dimensione del file da processare
#define MAX_CONCURRENT 5 // numero massimo di file concorrenti che il server può gestire
#define CTRL_MTYPE 99 // Tipo di messaggio controllo per modificare il limite dei file concorrenti
#define REQ_MTYPE 1 // Tipo di messaggio richiesta per client a server
#define RESP_MTYPE 2 // Tipo di messaggio risposta per server a client
#define MAX_FILENAME_LEN 256 // Lunghezza massima del nome del file

// Scheduling policies
#define SCHED_FCFS 0
#define SCHED_PRIORITY 1

// Messaggio che invia il client al server
struct msg_request {
    long mtype;
    size_t size;
    pid_t pid;
    int scheduling_policy;
};

// Messaggio che il server invia al client
struct msg_response {
    long mtype;
    char hash[65];
};

// Messaggio che permette di modificare il limite di file concorrenti
struct msg_control {
    long mtype;
    int new_limit;
};

#endif
