#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include <time.h>

#define MAP_NAME   "NexVaultTelemetrySharedMem"
#define SHM_SIZE   4096

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
    int    auth_status;
    int    data_ready;
} TelemetryRecord;

LARGE_INTEGER freq;

double get_wall_time() {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}

/* ── WRITER THREAD: Telemetry Ingestion Service ── */
DWORD WINAPI telemetry_writer(LPVOID param) {
    TelemetryRecord *shm = (TelemetryRecord*)param;

    printf("  [Telemetry Ingestion]  Vehicle 4821 connection received...\n");
    printf("  [Telemetry Ingestion]  Authenticating session...\n");
    Sleep(200);

    shm->vehicle_id         = 4821;
    shm->speed_kmh          = 67.3f;
    shm->latitude           = 27.7172;
    shm->longitude          = 85.3240;
    shm->lidar_range_m      = 42.5f;
    shm->camera_feed_active = 1;
    shm->timestamp_unix     = (long)time(NULL);
    shm->auth_status        = 1;
    strncpy(shm->event_type,    "NORMAL_OPERATION",   sizeof(shm->event_type)-1);
    strncpy(shm->session_token, "SESSION-NV-4821-XK9", sizeof(shm->session_token)-1);

    shm->data_ready = 1;

    printf("  [Telemetry Ingestion]  Data written to shared memory:\n");
    printf("    VehicleID   : %d\n",   shm->vehicle_id);
    printf("    Speed       : %.1f km/h\n", shm->speed_kmh);
    printf("    GPS         : %.4f, %.4f\n", shm->latitude, shm->longitude);
    printf("    LiDAR Range : %.1f m\n", shm->lidar_range_m);
    printf("    Auth Status : %s\n",    shm->auth_status == 1 ? "AUTHENTICATED" : "REJECTED");
    printf("    Session     : %s\n",    shm->session_token);
    printf("    Timestamp   : %ld\n",   shm->timestamp_unix);
    printf("  [Telemetry Ingestion]  Handoff complete.\n\n");

    return 0;
}

/* ── READER: Sensor Fusion + Downstream Services ── */
void telemetry_reader(TelemetryRecord *shm) {
    int waited = 0;
    while (!shm->data_ready && waited < 50) {
        Sleep(10);
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
    QueryPerformanceFrequency(&freq);

    printf("========================================================================================\n");
    printf("         NexVault Dynamics - IPC Shared Memory Demo (Windows)                         \n");
    printf("              CreateFileMapping / MapViewOfFile - Telemetry Pipeline                  \n");
    printf("========================================================================================\n\n");

    printf("System Configuration:\n");
    printf("  - IPC Method       : Windows File Mapping (CreateFileMapping + MapViewOfFile)\n");
    printf("  - Named Mapping    : %s\n", MAP_NAME);
    printf("  - Security         : SECURITY_ATTRIBUTES with NULL DACL (same session only)\n");
    printf("  - Record Size      : %zu bytes\n", sizeof(TelemetryRecord));
    printf("  - Platform         : Windows Server 2022\n\n");

    /* ── create named file mapping ── */
    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;  /* default security - current user only */
    sa.bInheritHandle       = FALSE;

    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,  /* backed by page file */
        &sa,
        PAGE_READWRITE,
        0, SHM_SIZE,
        MAP_NAME
    );

    if (hMap == NULL) {
        printf("ERROR: CreateFileMapping failed: %lu\n", GetLastError());
        return 1;
    }

    TelemetryRecord *shm = (TelemetryRecord*)MapViewOfFile(
        hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);

    if (shm == NULL) {
        printf("ERROR: MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(hMap);
        return 1;
    }

    ZeroMemory(shm, SHM_SIZE);

    printf("  [NexVault IPC]  Named file mapping created: %s\n", MAP_NAME);
    printf("  [NexVault IPC]  Security: SECURITY_ATTRIBUTES applied\n\n");
    printf("=== Starting IPC Pipeline Simulation ===\n\n");

    double start = get_wall_time();

    /* writer thread */
    HANDLE hWriter = CreateThread(NULL, 0, telemetry_writer, shm, 0, NULL);

    /* main thread reads after short delay */
    Sleep(300);
    telemetry_reader(shm);

    WaitForSingleObject(hWriter, INFINITE);
    CloseHandle(hWriter);

    double elapsed = get_wall_time() - start;

    printf("========================================================================================\n");
    printf("                    IPC PERFORMANCE METRICS (Windows)                                 \n");
    printf("========================================================================================\n");
    printf("  IPC Method         : Windows File Mapping\n");
    printf("  Data Transfer      : Zero-copy (direct memory access)\n");
    printf("  Named Mapping      : %s\n", MAP_NAME);
    printf("  Record Size        : %zu bytes\n", sizeof(TelemetryRecord));
    printf("  Total Pipeline Time: %.4f seconds\n", elapsed);
    printf("  Services Notified  : Sensor Fusion, Session Manager, Audit Logger, Watchdog\n");
    printf("  Security           : NULL DACL = current session only - no cross-user access\n");
    printf("========================================================================================\n");
    printf("  [NexVault IPC]  Vehicle 4821 handshake complete - all services notified.\n");
    printf("========================================================================================\n\n");

    /* cleanup */
    UnmapViewOfFile(shm);
    CloseHandle(hMap);

    printf("  File mapping closed and released.\n");
    printf("  Simulation complete. Compare with Linux IPC output.\n\n");

    return 0;
}