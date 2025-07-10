#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <openssl/sha.h>

int main(int argc, char *argv[]) {

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <shm_key> <data_size> <client_pid>\n", argv[0]);
        return 1;
    }

    int shm_key = atoi(argv[1]);
    size_t data_size = atoi(argv[2]);
    pid_t client_pid = atoi(argv[3]);

    printf("[WORKER] SHM KEY: %d (size: %zu) from SHM %d\n", shm_key, data_size, shm_key);

    // Access shared memory
    int shmid = shmget(shm_key, data_size, 0666);
    if (shmid == -1) {
        perror("shmget in worker");
        return 1;
    }

    char *file_data = shmat(shmid, NULL, 0);
    if (file_data == (char *)-1) {
        perror("shmat in worker");
        return 1;
    }

    uint8_t hash[32];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (uint8_t*)file_data, data_size);
    SHA256_Final(hash, &ctx);
    shmdt(file_data);

    // Rimuvoi la memoria condivisa
    shmctl(shmid, IPC_RMID, NULL);

    // Convert binary hash to hex string
    struct msg_response resp;
    resp.mtype = client_pid;
    for (int i = 0; i < 32; i++) {
        sprintf(resp.hash + i * 2, "%02x", hash[i]);
    }

    resp.hash[64] = '\0';

    // Get the message queue
    printf("Getting message queue with key %d\n", RESP_MSG_KEY);
     // The msgget() function returns the message queue identifier associated with the
     // message queue name, msg_key, and the permissions specified by msgflg.
    int msqid = msgget(RESP_MSG_KEY, 0666);
    if (msqid == -1) {
        perror("msgget");
        return 1;
    }
    
    // Send the response back to the client
    printf("Worker sending with mtype = %d\n", resp.mtype);
    printf("Message queue ID: %d\n", msqid);
    if (msgsnd(msqid, &resp, sizeof(resp) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        return 1;
    }

    printf("[WORKER] Response sent to client PID %d\n", client_pid);
    printf("\n-------------------------------------------------\n");

    return 0;
}
