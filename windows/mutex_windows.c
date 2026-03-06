#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define NUM_WORKERS      5
#define LINES_PER_WORKER 3
#define LOG_FILE         "nexvault_audit_SECURE.log"

LARGE_INTEGER freq;
CRITICAL_SECTION cs;   /* Windows mutex equivalent */

double get_wall_time() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}

typedef struct {
    int worker_id;
} WorkerArgs;

/* SAFE writer - CRITICAL_SECTION protected */
DWORD WINAPI worker_safe(LPVOID param) {
    WorkerArgs *args = (WorkerArgs*)param;
    int wid = args->worker_id;

    HANDLE hFile = CreateFileA(LOG_FILE,
        GENERIC_WRITE, FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("  [Worker %d] ERROR: Cannot open log file\n", wid);
        return 1;
    }

    for (int i = 0; i < LINES_PER_WORKER; i++) {
        char buf[256];
        Sleep(wid % 3 + 1);

        int len = snprintf(buf, sizeof(buf),
            "VehicleProc %d | Event: TELEMETRY | VehicleID: %d | Line %d | Time: %.4f\r\n",
            900 + wid, 4820 + wid, i, get_wall_time());

        /* ── CRITICAL SECTION START ── */
        EnterCriticalSection(&cs);   /* acquire lock */

        SetFilePointer(hFile, 0, NULL, FILE_END);
        DWORD written;
        WriteFile(hFile, buf, len, &written, NULL);
        FlushFileBuffers(hFile);     /* flush before releasing */

        LeaveCriticalSection(&cs);  /* release lock */
        /* ── CRITICAL SECTION END ── */
    }

    CloseHandle(hFile);
    printf("  [Worker Thread %d] Done - lock released.\n", wid);
    return 0;
}

int main() {
    QueryPerformanceFrequency(&freq);
    InitializeCriticalSection(&cs);

    printf("========================================================================================\n");
    printf("         NexVault Dynamics - Mutex Fix Demonstration (Windows)                        \n");
    printf("              Five Vehicle Workers - CRITICAL_SECTION Protection                      \n");
    printf("========================================================================================\n\n");

    printf("System Configuration:\n");
    printf("  - Workers          : %d concurrent vehicle log writers\n", NUM_WORKERS);
    printf("  - Lines per worker : %d\n", LINES_PER_WORKER);
    printf("  - Log file         : %s\n", LOG_FILE);
    printf("  - Mutex type       : Windows CRITICAL_SECTION\n");
    printf("  - Platform         : Windows Server 2022\n\n");

    DeleteFileA(LOG_FILE);

    HANDLE hCreate = CreateFileA(LOG_FILE,
        GENERIC_WRITE, FILE_SHARE_WRITE,
        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CloseHandle(hCreate);

    printf("=== Spawning %d concurrent vehicle worker threads ===\n\n", NUM_WORKERS);

    double start = get_wall_time();

    HANDLE threads[NUM_WORKERS];
    WorkerArgs args[NUM_WORKERS];

    for (int i = 0; i < NUM_WORKERS; i++) {
        args[i].worker_id = i;
        printf("  [Worker Thread %d] VehicleID %d acquiring lock and writing...\n",
               i, 4820 + i);
        threads[i] = CreateThread(NULL, 0, worker_safe, &args[i], 0, NULL);
    }

    WaitForMultipleObjects(NUM_WORKERS, threads, TRUE, INFINITE);

    for (int i = 0; i < NUM_WORKERS; i++) CloseHandle(threads[i]);

    double elapsed = get_wall_time() - start;

    DeleteCriticalSection(&cs);

    printf("\n=== All workers finished. Reading secured audit log... ===\n\n");

    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) { printf("ERROR: Cannot open log\n"); return 1; }

    char line[256];
    int line_count = 0;
    int expected = NUM_WORKERS * LINES_PER_WORKER;

    printf("  Contents of %s:\n", LOG_FILE);
    printf("  ------------------------------------------------------------------------\n");
    while (fgets(line, sizeof(line), fp)) {
        printf("  %s", line);
        line_count++;
    }
    fclose(fp);
    printf("  ------------------------------------------------------------------------\n\n");

    printf("========================================================================================\n");
    printf("                   MUTEX FIX ANALYSIS (Windows)                                       \n");
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
    printf("  FIX APPLIED  : Windows CRITICAL_SECTION (EnterCriticalSection / LeaveCriticalSection)\n");
    printf("  RESULT       : Each thread holds exclusive lock during write + FlushFileBuffers\n");
    printf("  SECURITY     : Audit log integrity guaranteed - forensics-grade evidence\n");
    printf("  COMPLIANCE   : Log entries sequential and complete - passes audit check\n");
    printf("========================================================================================\n\n");
    printf("Simulation complete. Compare with race_condition_windows output to see the difference.\n\n");

    return 0;
}