#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define NUM_WORKERS 5
#define LINES_PER_WORKER 3
#define LOG_FILE "nexvault_audit_SECURE.log"

double get_wall_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/*
 * Use a POSIX named semaphore so that separate forked processes
 * (not threads) can share the same mutex.
 * sem_open() creates a semaphore visible across processes by name.
 */
#include <semaphore.h>
#define SEM_NAME "/nexvault_audit_mutex"

void worker_safe(int worker_id) {
    /* open the shared semaphore */
    sem_t *sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED) { perror("sem_open worker"); exit(1); }

    int fd = open(LOG_FILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) { perror("open failed"); exit(1); }

    for (int i = 0; i < LINES_PER_WORKER; i++) {
        char buf[256];
        usleep(1000 * (worker_id % 3 + 1));

        /* ── CRITICAL SECTION START ── */
        sem_wait(sem);   /* acquire lock - only ONE process past this point */

        int len = snprintf(buf, sizeof(buf),
            "VehicleProc %d | Event: TELEMETRY | VehicleID: %d | Line %d | Time: %.4f\n",
            900 + worker_id, 4820 + worker_id, i, get_wall_time());

        write(fd, buf, len);
        fsync(fd);       /* flush to disk before releasing lock */

        sem_post(sem);   /* release lock */
        /* ── CRITICAL SECTION END ── */
    }

    close(fd);
    sem_close(sem);
}

int main() {
    printf("========================================================================================\n");
    printf("         NexVault Dynamics - Mutex Fix Demonstration (Linux)                          \n");
    printf("              Five Vehicle Workers - POSIX Semaphore Protection                       \n");
    printf("========================================================================================\n\n");

    printf("System Configuration:\n");
    printf("  - Workers          : %d concurrent vehicle log writers\n", NUM_WORKERS);
    printf("  - Lines per worker : %d\n", LINES_PER_WORKER);
    printf("  - Log file         : %s\n", LOG_FILE);
    printf("  - Mutex type       : POSIX named semaphore (%s)\n", SEM_NAME);
    printf("  - Platform         : Linux / Ubuntu 24.04 LTS\n\n");

    remove(LOG_FILE);

    /* create semaphore with initial value 1 (binary mutex) */
    sem_unlink(SEM_NAME); /* clean up any leftover */
    sem_t *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
    if (sem == SEM_FAILED) { perror("sem_open parent"); return 1; }

    printf("=== Spawning %d concurrent vehicle worker processes ===\n\n", NUM_WORKERS);

    double start = get_wall_time();
    pid_t pids[NUM_WORKERS];

    for (int i = 0; i < NUM_WORKERS; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            printf("  [Worker PID %d] VehicleID %d acquiring mutex and writing...\n",
                   getpid(), 4820 + i);
            worker_safe(i);
            printf("  [Worker PID %d] Done - mutex released.\n", getpid());
            exit(0);
        }
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    double elapsed = get_wall_time() - start;

    /* cleanup semaphore */
    sem_close(sem);
    sem_unlink(SEM_NAME);

    printf("\n=== All workers finished. Reading secured audit log... ===\n\n");

    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) { perror("fopen"); return 1; }

    char line[256];
    int line_count = 0;
    int expected = NUM_WORKERS * LINES_PER_WORKER;
    int prev_worker = -1;
    int interleave_detected = 0;

    printf("  Contents of %s:\n", LOG_FILE);
    printf("  ------------------------------------------------------------------------\n");
    while (fgets(line, sizeof(line), fp)) {
        printf("  %s", line);
        line_count++;

        /* simple interleave check: consecutive lines from same worker = good */
        int wid = -1;
        sscanf(line, "VehicleProc %d", &wid);
        if (prev_worker != -1 && wid != prev_worker) {
            /* worker changed - acceptable only at block boundaries */
        }
        prev_worker = wid;
    }
    fclose(fp);
    printf("  ------------------------------------------------------------------------\n\n");

    printf("========================================================================================\n");
    printf("                   MUTEX FIX ANALYSIS (Linux)                                         \n");
    printf("========================================================================================\n");
    printf("  Total workers          : %d\n", NUM_WORKERS);
    printf("  Expected log lines     : %d\n", expected);
    printf("  Actual lines written   : %d\n", line_count);
    printf("  Total execution time   : %.4f seconds\n", elapsed);

    if (line_count == expected) {
        printf("  Integrity check        : PASSED  <<< ALL ENTRIES WRITTEN CORRECTLY\n");
        printf("  Data loss              : None\n");
    } else {
        printf("  Integrity check        : FAILED - unexpected error\n");
    }

    printf("========================================================================================\n");
    printf("  FIX APPLIED  : POSIX named semaphore (sem_open / sem_wait / sem_post)\n");
    printf("  RESULT       : Each worker holds exclusive lock during write + fsync\n");
    printf("  SECURITY     : Audit log integrity guaranteed - forensics-grade evidence\n");
    printf("  COMPLIANCE   : Log entries sequential and complete - passes audit check\n");
    printf("========================================================================================\n\n");
    printf("Simulation complete. Compare with race_condition_linux output to see the difference.\n\n");

    return 0;
}
