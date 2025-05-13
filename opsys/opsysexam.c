#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define PIPE_READ 0
#define PIPE_WRITE 1
#define MSG_SIZE 256

struct msgbuf {
    long mtype;
    char mtext[MSG_SIZE];
};

void error(char *msg) {
    perror(msg);
    exit(1);
}

int main() {
    int pipefd[2];
    int msgid;
    int shmid;
    struct sembuf sop;
    key_t key;

    char thesis_info[MSG_SIZE] = "Thesis Title: Harnessing AI, Student Name: John Doe, Year: 2024, Supervisor: Dr. Smith";

    
    if (pipe(pipefd) == -1) 
        error("pipe");

    
    key = ftok("program", 'b');
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1)
        error("msgget");

    
    shmid = shmget(key, sizeof(int), 0666 | IPC_CREAT);
    if (shmid == -1)
        error("shmget");

    
    int semid = semget(key, 1, 0666 | IPC_CREAT);
    if (semid == -1)
        error("semget");
    semctl(semid, 0, SETVAL, 1);

    pid_t pid = fork();
    if (pid == -1) 
        error("fork");

    if (pid == 0) { 
        close(pipefd[PIPE_READ]); 

        printf("Student logged in successfully.\n");

        
        write(pipefd[PIPE_WRITE], thesis_info, strlen(thesis_info) + 1);
        printf("Student uploads: %s\n", thesis_info);
        close(pipefd[PIPE_WRITE]);

        
        struct msgbuf buf;
        if (msgrcv(msgid, &buf, MSG_SIZE, 1, 0) >= 0) {
            printf("Student received question: %s\n", buf.mtext);  
        } else {
            printf("Failed to receive message.\n");
        }

        exit(0);
    } else {  
        close(pipefd[PIPE_WRITE]); 

        printf("Topic leader logged in successfully.\n");

        
        char buffer[MSG_SIZE];
        if (read(pipefd[PIPE_READ], buffer, MSG_SIZE) > 0) {
            printf("Topic leader reads thesis info: %s\n", buffer);
        } else {
            printf("Failed to read from pipe.\n");
        }
        close(pipefd[PIPE_READ]);

        
        struct msgbuf buf = {1, "What technology do you want to use to complete your assignment?"};
        if (msgsnd(msgid, &buf, strlen(buf.mtext) + 1, 0) == -1) {
            printf("Failed to send message.\n");
        } else {
            printf("Topic leader sent question to student.\n");
        }

        
        srand(time(NULL));
        int decision = rand() % 5 != 0; 

        
        sop.sem_num = 0;
        sop.sem_op = -1; 
        sop.sem_flg = 0;
        semop(semid, &sop, 1);

        int *shared_mem = shmat(shmid, NULL, 0);
        *shared_mem = decision;
        printf("Topic leader makes a decision: %s\n", decision ? "Accepted" : "Rejected");
        shmdt(shared_mem);

        sop.sem_op = 1; 
        semop(semid, &sop, 1);

        wait(NULL); 

        
        msgctl(msgid, IPC_RMID, NULL);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);
    }

    return 0;
}