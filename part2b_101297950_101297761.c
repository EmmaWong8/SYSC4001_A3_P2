#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>

#define NUM_QUESTIONS 5
#define MAX_EXAMS 20
#define MAX_NAME 256

typedef struct {
    char rubric[NUM_QUESTIONS]; // rubric letters/answers
    int  current_student;  // student number of current exam
    int  question_state[NUM_QUESTIONS]; // 0 = not started, 1 = in progress, 2 = done
    int  current_exam_index; // index into exam_files[]
    int  finished; // 1 when we should stop all TAs

    // semaphores for synchronization 
    sem_t mutex; // protects shared state (question_state, exam_index, finished, current_student)
    sem_t rubric_mutex; // protects rubric[] and rubric file
} SharedData;

// Helper functions

// Trim newline from a string read by fgets
void trim_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

// Load rubric into shared memory
void load_rubric(const char *path, SharedData *shared) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("Cannot open rubric file");
        exit(1);
    }

    int qnum;
    char comma;
    char letter;

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        if (fscanf(f, "%d %c %c", &qnum, &comma, &letter) != 3) {
            fprintf(stderr, "Rubric file format error on line %d\n", i + 1);
            fclose(f);
            exit(1);
        }
        if (qnum < 1 || qnum > NUM_QUESTIONS) {
            fprintf(stderr, "Rubric question number out of range\n");
            fclose(f);
            exit(1);
        }
        shared->rubric[qnum - 1] = letter;
    }

    fclose(f);
}

// Rewrite rubric.txt based on shared->rubric[]
void save_rubric(const char *path, SharedData *shared) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("Cannot open rubric file for writing");
        exit(1);
    }

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        fprintf(f, "%d, %c\n", i + 1, shared->rubric[i]);
    }

    fclose(f);
}

// Load exam file at exam_files[index], update shared data
void load_exam(const char *exam_path, SharedData *shared) {
    FILE *f = fopen(exam_path, "r");
    if (!f) {
        perror("Cannot open exam file");
        exit(1);
    }

    int student;
    if (fscanf(f, "%d", &student) != 1) {
        fprintf(stderr, "Exam file %s missing student number\n", exam_path);
        fclose(f);
        exit(1);
    }
    fclose(f);

    shared->current_student = student;

    // Reset question states
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shared->question_state[i] = 0;
    }

    // If student 9999 then mark as finished
    if (student == 9999) {
        shared->finished = 1;
    }
}

// Return 1 if all questions are done (state == 2)
int all_questions_done(SharedData *shared) {
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        if (shared->question_state[i] != 2) {
            return 0;
        }
    }
    return 1;
}

// Pick a question to mark, otherwise -1
// Now synchronized using shared->mutex
int select_question(SharedData *shared) {
    int q = -1;

    sem_wait(&shared->mutex);
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        if (shared->question_state[i] == 0) {
            shared->question_state[i] = 1; // mark as in-progress
            q = i;
            break;
        }
    }
    sem_post(&shared->mutex);

    return q;
}

// Sleep between 0.5 and 1.0 seconds
void random_short_delay() {
    int ms = 500 + rand() % 501; // 500-1000 ms
    usleep(ms * 1000);
}

// Sleep between 1.0 and 2.0 seconds
void random_mark_delay() {
    int ms = 1000 + rand() % 1001; // 1000-2000 ms
    usleep(ms * 1000);
}

// Try to load the next exam
void try_load_next_exam(SharedData *shared,
                        char exam_files[][MAX_NAME],
                        int num_exams) {

    sem_wait(&shared->mutex);

    if (shared->finished) {
        sem_post(&shared->mutex);
        return;
    }

    // Only load next exam if all questions are done
    if (!all_questions_done(shared)) {
        sem_post(&shared->mutex);
        return;
    }

    int next_idx = shared->current_exam_index + 1;

    if (next_idx >= num_exams) {
        printf("[Loader] No more exams. Setting finished.\n");
        fflush(stdout);
        shared->finished = 1;
        sem_post(&shared->mutex);
        return;
    }

    printf("[Loader] Loading next exam index %d (%s)\n",
           next_idx, exam_files[next_idx]);
    fflush(stdout);

    // Call load_exam while still holding mutex
    load_exam(exam_files[next_idx], shared);
    shared->current_exam_index = next_idx;

    sem_post(&shared->mutex);
}

// TA process function
void ta_process(int ta_id,
                SharedData *shared,
                char exam_files[][MAX_NAME],
                int num_exams,
                const char *rubric_path) {

    srand((unsigned int)(time(NULL) ^ getpid()));

    while (1) {

        sem_wait(&shared->mutex);
        int finished = shared->finished;
        int student  = shared->current_student;
        sem_post(&shared->mutex);

        if (finished || student == 9999) {
            printf("[TA %d] Detected finished or student 9999. Exiting.\n", ta_id);
            fflush(stdout);
            break;
        }

        printf("[TA %d] Starting exam for student %04d (exam index %d)\n",
               ta_id, student, shared->current_exam_index);
        fflush(stdout);

        // Rubric review
        for (int q = 0; q < NUM_QUESTIONS; q++) {
            char current_letter;

            // Read rubric letter under mutex
            sem_wait(&shared->mutex);
            current_letter = shared->rubric[q];
            sem_post(&shared->mutex);

            printf("[TA %d] Checking rubric for question %d (current:%c)\n",
                   ta_id, q + 1, current_letter);
            fflush(stdout);

            random_short_delay(); // 0.5 - 1.0 seconds

            int need_correction = rand() % 2; // 0 or 1
            if (need_correction) {
                // Protext rubric update and file write
                sem_wait(&shared->rubric_mutex);

                char old = shared->rubric[q];
                char newc = old + 1; // next ASCII
                shared->rubric[q] = newc;

                printf("[TA %d] CORRECTING rubric Q%d: %c -> %c\n",
                       ta_id, q + 1, old, newc);
                fflush(stdout);

                save_rubric(rubric_path, shared);

                sem_post(&shared->rubric_mutex);
            }
        }

        // Mark questions
        while (1) {
            int q = select_question(shared);
            if (q == -1) {
                // No more questions to mark
                break;
            }

            // Read student safely for printing
            sem_wait(&shared->mutex);
            int cur_student = shared->current_student;
            sem_post(&shared->mutex);

            printf("[TA %d] Marking student %04d question %d\n",
                   ta_id, cur_student, q + 1);
            fflush(stdout);

            random_mark_delay(); // 1.0-2.0 seconds

            // Set question to done under mutex
            sem_wait(&shared->mutex);
            shared->question_state[q] = 2; // done
            sem_post(&shared->mutex);
        }

        printf("[TA %d] Finished exam for student %04d\n", ta_id, student);
        fflush(stdout);

        // Try to load the next exam 
        try_load_next_exam(shared, exam_files, num_exams);
    }

 
    shmdt(shared);
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num_TAs>\n", argv[0]);
        return 1;
    }

    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        fprintf(stderr, "Please use at least 2 TAs.\n");
        return 1;
    }

    const char *rubric_path = "rubric.txt";

    // Read exam file names from exams_list.txt
    FILE *list = fopen("exams_list.txt", "r");
    if (!list) {
        perror("Cannot open exams_list.txt");
        return 1;
    }

    char exam_files[MAX_EXAMS][MAX_NAME];
    int num_exams = 0;
    while (num_exams < MAX_EXAMS &&
           fgets(exam_files[num_exams], MAX_NAME, list)) {
        trim_newline(exam_files[num_exams]);
        if (exam_files[num_exams][0] == '\0') {
            continue; // skip empty lines
        }
        num_exams++;
    }
    fclose(list);

    if (num_exams == 0) {
        fprintf(stderr, "No exams listed in exams_list.txt\n");
        return 1;
    }

    printf("Loaded %d exam file names.\n", num_exams);

    // Create shared memory segment
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }

    SharedData *shared = (SharedData *) shmat(shmid, NULL, 0);
    if (shared == (void *) -1) {
        perror("shmat failed");
        return 1;
    }

    // Initialize shared memory
    shared->finished = 0;
    shared->current_exam_index = 0;
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        shared->question_state[i] = 0;
    }

    // initialize semaphores (pshared = 1 means shared between processes)
    if (sem_init(&shared->mutex, 1, 1) != 0) {
        perror("sem_init mutex failed");
        return 1;
    }
    if (sem_init(&shared->rubric_mutex, 1, 1) != 0) {
        perror("sem_init rubric_mutex failed");
        return 1;
    }

    load_rubric(rubric_path, shared);
    load_exam(exam_files[0], shared); // first exam

    printf("Initial student: %04d\n", shared->current_student);

    // Fork TA processes
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            return 1;
        }
        if (pid == 0) {
            // Child TA process
            ta_process(i, shared, exam_files, num_exams, rubric_path);
            exit(0);
        }
    }

    // Parent waits for all TAs
    for (int i = 0; i < num_tas; i++) {
        wait(NULL);
    }

    // Destroy semaphores in parent after children finish
    sem_destroy(&shared->mutex);
    sem_destroy(&shared->rubric_mutex);

    // Parent detach and remove shared memory
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);

    printf("All TAs have finished. Exiting part2b.\n");
    return 0;
}
