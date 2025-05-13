#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define TEAM_COUNT 2
#define JERSEY_OPTIONS 3
#define TICKET_OPTIONS 3
#define SEAT_CAPACITY 90000

struct msg_buffer {
    long msg_type;
    int data[2]; 
};

struct shared_memory {
    int vip_seats;
    int tickets_sold;
};

void sem_wait(int sem_id) {
    struct sembuf sem_op;
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}

void sem_signal(int sem_id) {
    struct sembuf sem_op;
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    semop(sem_id, &sem_op, 1);
}

int main() {
    srand(time(NULL) ^ getpid());

    key_t key_msg = ftok("/tmp", 'a');
    int msg_id = msgget(key_msg, IPC_CREAT | 0666);

    key_t key_shm = ftok("/tmp", 'b');
    int shm_id = shmget(key_shm, sizeof(struct shared_memory), IPC_CREAT | 0666);
    struct shared_memory *shm_ptr = (struct shared_memory *) shmat(shm_id, NULL, 0);

    key_t key_sem = ftok("/tmp", 'c');
    int sem_id = semget(key_sem, 1, IPC_CREAT | 0666);
    semctl(sem_id, 0, SETVAL, 1);

    shm_ptr->vip_seats = SEAT_CAPACITY / 2;
    shm_ptr->tickets_sold = 0;

    int team_pipes[TEAM_COUNT][2];
    for (int i = 0; i < TEAM_COUNT; i++) {
        if (pipe(team_pipes[i]) == -1) {
            perror("Pipe failed");
            return 1;
        }
    }

    pid_t pid;
    for (int i = 0; i < TEAM_COUNT; i++) {
        pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return 1;
        } else if (pid == 0) {
            srand(time(NULL) ^ getpid());
            close(team_pipes[i][0]); 
            int jersey_choice = rand() % JERSEY_OPTIONS;
            int ticket_choice = rand() % TICKET_OPTIONS;
            printf("Team %d: Jersey choice: %d, Ticket choice: %d\n", i, jersey_choice, ticket_choice);
            int choices[2] = {jersey_choice, ticket_choice};
            write(team_pipes[i][1], choices, sizeof(choices)); 
            close(team_pipes[i][1]); 
            exit(0);
        } else {
            close(team_pipes[i][1]); 
        }
    }

    int choices[TEAM_COUNT][2];
    for (int i = 0; i < TEAM_COUNT; i++) {
        read(team_pipes[i][0], choices[i], sizeof(choices[i])); 
        close(team_pipes[i][0]);
    }

    printf("Dortmund Jersey: %d, Tickets: %d\n", choices[0][0], choices[0][1]);
    printf("Real Madrid Jersey: %d, Tickets: %d\n", choices[1][0], choices[1][1]);

    sem_wait(sem_id);
    shm_ptr->vip_seats -= (choices[0][1] + choices[1][1]);
    shm_ptr->tickets_sold += (choices[0][1] + choices[1][1]);
    sem_signal(sem_id);

    printf("Dortmund: %d tickets, %d VIP seats\n", choices[0][1], choices[0][1]);
    printf("Real Madrid: %d tickets, %d VIP seats\n", choices[1][1], choices[1][1]);
    printf("Remaining VIP seats: %d\n", shm_ptr->vip_seats);
    printf("Tickets sold by organizer: %d\n", shm_ptr->tickets_sold);

    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    msgctl(msg_id, IPC_RMID, NULL);

    return 0;
}
