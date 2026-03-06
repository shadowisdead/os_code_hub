#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

#define SHM_NAME    "/nexvault_telemetry_shm"
#define SEM_WRITE   "/nexvault_sem_write"
#define SEM_READ    "/nexvault_sem_read"
#define SHM_SIZE    4096

/* Telemetry record shared between NexVault services */
typedef struct {
    int    vehicle_id;
    float  speed_kmh;
    double latitude;
    double longitude;
    float  lidar_range_m;
    int    camera_feed_active;
    char   event_type[32];
    char   session_token[64];
    long   timestamp_unix;
    int    auth_status;       /* 0=pending, 1=authenticated, 2=rejected */
    int    data_ready;        /* flag: 1 = writer finished, safe to read */
} TelemetryRecord;

double get_wall_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* ── WRITER PROCESS: Telemetry Ingestion Service ── */
void telemetry_writer(TelemetryRecord *shm) {
    printf("  [Telemetry Ingestion]  Vehicle 4821 connection received...\n");
    printf("  [Telemetry Ingestion]  Authenticating session...\n");
    usleep(200000); /* simulate auth delay */

    shm->vehicle_id        = 4821;
    shm->speed_kmh         = 67.3f;
    shm->latitude          = 27.7172;
    shm->longitude         = 85.3240;
    shm->lidar_range_m     = 42.5f;
    shm->camera_feed_active = 1;
    shm->timestamp_unix    = (long)time(NULL);
    shm->auth_status       = 1;
    strncpy(shm->event_type,    "NORMAL_OPERATION",  sizeof(shm->event_type)-1);
    strncpy(shm->session_token, "SESSION-NV-4821-XK9", sizeof(shm->session_token)-1);

    shm->data_ready = 1; /* signal that data is written */

    printf("  [Telemetry Ingestion]  Data written to shared memory:\n");
    printf("    VehicleID   : %d\n",   shm->vehicle_id);
    printf("    Speed       : %.1f km/h\n", shm->speed_kmh);
    printf("    GPS         : %.4f, %.4f\n", shm->latitude, shm->longitude);
    printf("    LiDAR Range : %.1f m\n", shm->lidar_range_m);
    printf("    Auth Status : %s\n",    shm->auth_status == 1 ? "AUTHENTICATED" : "REJECTED");
    printf("    Session     : %s\n",    shm->session_token);
    printf("    Timestamp   : %ld\n",   shm->timestamp_unix);
    printf("  [Telemetry Ingestion]  Handoff complete.\n\n");
}

/* ── READER PROCESS: Sensor Fusion + Session Manager ── */
void telemetry_reader(TelemetryRecord *shm) {
    /* wait until data is ready */
    int waited = 0;
    while (!shm->data_ready && waited < 50) {
        usleep(10000);
        waited++;
    }

    if (!shm->data_ready) {
        printf("  [Sensor Fusion]  ERROR: Timeout waiting for telemetry data!\n");
        return;
    }

    printf("  [Sensor Fusion Engine]  Reading from shared memory...\n");
    printf("    VehicleID   : %d\n",   shm->vehicle_id);
    printf("    Speed       : %.1f km/h\n", shm->speed_kmh);
    printf("    LiDAR Range : %.1f m\n", shm->lidar_range_m);
    printf("    Camera      : %s\n",    shm->camera_feed_active ? "ACTIVE" : "INACTIVE");
    printf("    Event       : %s\n",    shm->event_type);
    printf("  [Sensor Fusion Engine]  Fusion processing complete.\n\n");

    printf("  [Session Manager]  Registering vehicle session...\n");
    printf("    Session Token  : %s\n", shm->session_token);
    printf("    Auth Status    : %s\n", shm->auth_status == 1 ? "OK" : "FAIL");
    printf("    GPS Location   : %.4f N, %.4f E\n", shm->latitude, shm->longitude);
    printf("  [Session Manager]  Session registered successfully.\n\n");

    printf("  [Audit Logger]  Recording event to vehicle_events.log...\n");
    printf("    Entry: VehicleID:%d | Auth:%s | Session:%s | Time:%ld\n",
           shm->vehicle_id,
           shm->auth_status == 1 ? "OK" : "FAIL",
           shm->session_token,
           shm->timestamp_unix);
    printf("  [Audit Logger]  Event recorded.\n\n");

    printf("  [Safety Watchdog]  Checking vehicle health metrics...\n");
    printf("    Speed        : %.1f km/h  -> %s\n",
           shm->speed_kmh,
           shm->speed_kmh < 120.0f ? "NORMAL" : "OVERSPEED WARNING");
    printf("    LiDAR        : %.1f m     -> %s\n",
           shm->lidar_range_m,
           shm->lidar_range_m > 10.0f ? "CLEAR" : "OBSTACLE DETECTED");
    printf("  [Safety Watchdog]  Vehicle 4821 status: NORMAL.\n\n");
}

int main() {
    printf("========================================================================================\n");
    printf("         NexVault Dynamics - IPC Shared Memory Demo (Linux)                           \n");
    printf("              POSIX shm_open / mmap - Telemetry Pipeline                              \n");
    printf("========================================================================================\n\n");

    printf("System Configuration:\n");
    printf("  - IPC Method       : POSIX Shared Memory (shm_open + mmap)\n");
    printf("  - Shared Object    : %s\n", SHM_NAME);
    printf("  - Permissions      : 0600 (owner read/write only)\n");
    printf("  - Record Size      : %zu bytes\n", sizeof(TelemetryRecord));
    printf("  - Platform         : Linux / Ubuntu 24.04 LTS\n\n");

    /* ── create shared memory object ── */
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd == -1) { perror("shm_open"); return 1; }

    if (ftruncate(fd, SHM_SIZE) == -1) { perror("ftruncate"); return 1; }

    TelemetryRecord *shm = mmap(NULL, SHM_SIZE,
                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }

    memset(shm, 0, SHM_SIZE);

    printf("  [NexVault IPC]  Shared memory object created: %s\n", SHM_NAME);
    printf("  [NexVault IPC]  Permissions: 0600 (owner-only access enforced)\n\n");
    printf("=== Starting IPC Pipeline Simulation ===\n\n");

    double start = get_wall_time();

    pid_t writer_pid = fork();
    if (writer_pid == 0) {
        /* child: writer */
        telemetry_writer(shm);
        exit(0);
    }

    /* parent waits briefly then reads */
    usleep(300000);
    telemetry_reader(shm);

    int status;
    waitpid(writer_pid, &status, 0);

    double elapsed = get_wall_time() - start;

    printf("========================================================================================\n");
    printf("                    IPC PERFORMANCE METRICS (Linux)                                   \n");
    printf("========================================================================================\n");
    printf("  IPC Method         : POSIX Shared Memory\n");
    printf("  Data Transfer      : Zero-copy (no kernel buffer involved)\n");
    printf("  Shared Object      : %s\n", SHM_NAME);
    printf("  Record Size        : %zu bytes\n", sizeof(TelemetryRecord));
    printf("  Total Pipeline Time: %.4f seconds\n", elapsed);
    printf("  Services Notified  : Sensor Fusion, Session Manager, Audit Logger, Watchdog\n");
    printf("  Security           : shm permissions 0600 - no other user can attach\n");
    printf("========================================================================================\n");
    printf("  [NexVault IPC]  Vehicle 4821 handshake complete - all services notified.\n");
    printf("========================================================================================\n\n");

    /* cleanup */
    munmap(shm, SHM_SIZE);
    close(fd);
    shm_unlink(SHM_NAME);

    printf("  Shared memory object removed: %s\n", SHM_NAME);
    printf("  Simulation complete. Compare with Windows IPC output.\n\n");

    return 0;
}
