#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>

typedef struct {
    int pid;
    char task_name[50];
    int arrival_time;
    int burst_time;
    double calc_start_time;
    double calc_completion_time;
    double calc_turnaround_time;
    double calc_waiting_time;
    double calc_response_time;
} Process;

double get_wall_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void cpu_intensive_work(int seconds) {
    double start = get_wall_time();
    double target = start + seconds;
    volatile double result = 0.0;
    while (get_wall_time() < target) {
        for (volatile long i = 0; i < 10000000; i++) {
            result += i * 3.14159;
            result = result / 2.71828;
        }
    }
}
void execute_process(Process *p) {
    pid_t child = fork();
    if (child == 0) {
        cpu_intensive_work(p->burst_time);
        exit(0);
    } else {
        int status;
        waitpid(child, &status, 0);
    }
}

void calculate_fcfs_times(Process p[], int n) {
    double current_time = 0;
    for (int i = 0; i < n; i++) {
        if (current_time < p[i].arrival_time)
            current_time = p[i].arrival_time;
        p[i].calc_start_time      = current_time;
        p[i].calc_completion_time = current_time + p[i].burst_time;
        p[i].calc_turnaround_time = p[i].calc_completion_time - p[i].arrival_time;
        p[i].calc_waiting_time    = p[i].calc_start_time - p[i].arrival_time;
        p[i].calc_response_time   = p[i].calc_start_time - p[i].arrival_time;
        current_time = p[i].calc_completion_time;
    }
}
void print_table(Process p[], int n) {
    printf("\n========================================================================================\n");
    printf("              NEXVAULT DYNAMICS - FCFS SCHEDULING EXECUTION TABLE                      \n");
    printf("                          (Theoretical Calculations)                                   \n");
    printf("========================================================================================\n");
    printf("PID  Task Name                    AT    BT    ST      CT      TAT     WT      RT\n");
    printf("----------------------------------------------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("P%-3d %-26s %-5d %-5d %-7.2f %-7.2f %-7.2f %-7.2f %-7.2f\n",
               p[i].pid, p[i].task_name, p[i].arrival_time, p[i].burst_time,
               p[i].calc_start_time, p[i].calc_completion_time,
               p[i].calc_turnaround_time, p[i].calc_waiting_time,
               p[i].calc_response_time);
    }
    printf("========================================================================================\n");
    printf("AT=Arrival Time  BT=Burst Time  ST=Start Time  CT=Completion Time\n");
    printf("TAT=Turnaround Time  WT=Waiting Time  RT=Response Time  (all in seconds)\n");
    printf("Note: Theoretical values are identical across OS platforms\n");
    printf("========================================================================================\n");
}
void calculate_metrics(Process p[], int n, double total_actual_time, int context_switches) {
    double tw=0, tt=0, tr=0, mw=0, mt=0, tb=0;
    for (int i = 0; i < n; i++) {
        tw += p[i].calc_waiting_time;
        tt += p[i].calc_turnaround_time;
        tr += p[i].calc_response_time;
        tb += p[i].burst_time;
        if (p[i].calc_waiting_time    > mw) mw = p[i].calc_waiting_time;
        if (p[i].calc_turnaround_time > mt) mt = p[i].calc_turnaround_time;
    }
    printf("\n========================================================================================\n");
    printf("               PERFORMANCE METRICS (Linux - FCFS) Actual OS Measurements               \n");
    printf("========================================================================================\n");
    printf("Average Waiting Time       = %.2f seconds\n", tw / n);
    printf("Average Response Time      = %.2f seconds\n", tr / n);
    printf("Average Turnaround Time    = %.2f seconds\n", tt / n);
    printf("Max Waiting Time           = %.2f seconds\n", mw);
    printf("Max Turnaround Time        = %.2f seconds\n", mt);
    printf("Throughput                 = %.4f processes/second\n", n / total_actual_time);
    printf("CPU Utilization            = %.2f%%\n", (tb / total_actual_time) * 100.0);
    printf("Total Execution Time       = %.4f seconds\n", total_actual_time);
    printf("Context Switches           = %d\n", context_switches);
    printf("========================================================================================\n");
    printf("Note: Execution time differs from Windows due to Linux kernel fork() overhead model\n");
    printf("========================================================================================\n");
}
int main() {
    Process processes[] = {
        {1,  "Telemetry Ingestion",   0,  5, 0,0,0,0,0},
        {2,  "Sensor Fusion Engine",  1,  3, 0,0,0,0,0},
        {3,  "ML Model Retraining",   2,  8, 0,0,0,0,0},
        {4,  "GPS Sync Service",      1,  2, 0,0,0,0,0},
        {5,  "Collision Detection",   3,  4, 0,0,0,0,0},
        {6,  "Safety Watchdog",       5,  1, 0,0,0,0,0},
        {7,  "CAN Bus Parser",        4,  2, 0,0,0,0,0},
        {8,  "Audit Logger",          4,  2, 0,0,0,0,0},
        {9,  "Route Analytics",       7,  6, 0,0,0,0,0},
        {10, "Fleet Reporter",        6,  5, 0,0,0,0,0}
    };
    int n = sizeof(processes) / sizeof(processes[0]);
    printf("========================================================================================\n");
    printf("         NexVault Dynamics - Process Scheduler (Linux/UNIX)                            \n");
    printf("                Algorithm: First-Come-First-Serve (FCFS)                               \n");
    printf("========================================================================================\n\n");
    printf("System Configuration:\n");
    printf("  - Total Processes  : %d\n", n);
    printf("  - Scheduling Policy: FCFS (Non-preemptive)\n");
    printf("  - Execution Mode   : Kernel-level process scheduling via fork()\n");
    printf("  - Platform         : Linux / Ubuntu 24.04 LTS\n\n");
    calculate_fcfs_times(processes, n);
    printf("--- NexVault Process Queue (Arrival Order) ---\n");
    printf("PID  Task Name                              Arrival Time   Burst Time\n");
    printf("-----------------------------------------------------------------------\n");
    for (int i = 0; i < n; i++)
        printf("P%-3d %-38s %-13ds %ds\n",
               processes[i].pid, processes[i].task_name,
               processes[i].arrival_time, processes[i].burst_time);
    printf("\n=== Starting FCFS Scheduling Simulation ===\n");
    printf("Measuring actual OS execution performance...\n\n");
    double sim_start = get_wall_time();
    int context_switches = 0;
    for (int i = 0; i < n; i++) {
        double elapsed = get_wall_time() - sim_start;
        if (processes[i].arrival_time > elapsed)
            sleep((int)(processes[i].arrival_time - elapsed));
        printf(">>  Executing P%d: %s\n", processes[i].pid, processes[i].task_name);
        execute_process(&processes[i]);
        context_switches++;
        printf("OK  Completed P%d\n\n", processes[i].pid);
    }
    double total_time = get_wall_time() - sim_start;
    print_table(processes, n);
    calculate_metrics(processes, n, total_time, context_switches);
    printf("\nSimulation complete. Compare output with Windows version for OS benchmarking.\n\n");
    return 0;
}
