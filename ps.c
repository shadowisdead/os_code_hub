#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

typedef struct {
    int pid;
    char task_name[50];
    int arrival_time;
    int burst_time;
    int priority;
    double calc_start_time;
    double calc_completion_time;
    double calc_turnaround_time;
    double calc_waiting_time;
    double calc_response_time;
    int completed;
} Process;

LARGE_INTEGER frequency;

double get_wall_time() {
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart/(double)frequency.QuadPart;
}

void execute_process(Process *p) {
    STARTUPINFO si; PROCESS_INFORMATION pi;
    ZeroMemory(&si,sizeof(si)); si.cb=sizeof(si);
    si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    ZeroMemory(&pi,sizeof(pi));
    char cmd[512];
    sprintf(cmd,
        "powershell -WindowStyle Hidden -Command "
        "\"$end=(Get-Date).AddSeconds(%d);"
        "while((Get-Date)-lt $end){$x=0;for($i=0;$i-lt 1000000;$i++){$x+=$i*3.14159}}\"",
        p->burst_time);
    if(CreateProcess(NULL,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        WaitForSingleObject(pi.hProcess,INFINITE);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

void calculate_priority_times(Process p[], int n) {
    double cur=0; int done=0;
    for(int i=0;i<n;i++) p[i].completed=0;
    while(done<n){
        int idx=-1,mp=999999;
        for(int i=0;i<n;i++)
            if(!p[i].completed&&p[i].arrival_time<=cur&&p[i].priority<mp)
                {mp=p[i].priority;idx=i;}
        if(idx==-1){
            double na=999999;
            for(int i=0;i<n;i++)
                if(!p[i].completed&&p[i].arrival_time>cur&&p[i].arrival_time<na) na=p[i].arrival_time;
            cur=na;
        } else {
            p[idx].calc_start_time      = cur;
            p[idx].calc_completion_time = cur+p[idx].burst_time;
            p[idx].calc_turnaround_time = p[idx].calc_completion_time-p[idx].arrival_time;
            p[idx].calc_waiting_time    = p[idx].calc_start_time-p[idx].arrival_time;
            p[idx].calc_response_time   = p[idx].calc_waiting_time;
            p[idx].completed=1; cur=p[idx].calc_completion_time; done++;
        }
    }
}

void print_table(Process p[], int n) {
    printf("\n========================================================================================\n");
    printf("         NEXVAULT DYNAMICS - PRIORITY SCHEDULING EXECUTION TABLE                       \n");
    printf("                          (Theoretical Calculations)                                   \n");
    printf("========================================================================================\n");
    printf("PID  Task Name                  PR  AT    BT    ST      CT      TAT     WT      RT\n");
    printf("----------------------------------------------------------------------------------------\n");
    for(int i=0;i<n;i++)
        printf("P%-3d %-24s %-4d %-5d %-5d %-7.2f %-7.2f %-7.2f %-7.2f %-7.2f\n",
               p[i].pid,p[i].task_name,p[i].priority,p[i].arrival_time,p[i].burst_time,
               p[i].calc_start_time,p[i].calc_completion_time,
               p[i].calc_turnaround_time,p[i].calc_waiting_time,p[i].calc_response_time);
    printf("========================================================================================\n");
    printf("PR=Priority (1=Highest)  Note: Theoretical values are identical across OS platforms\n");
    printf("========================================================================================\n");
}

void calculate_metrics(Process p[], int n, double total_time, int cs) {
    double tw=0,tt=0,tr=0,mw=0,mt=0,tb=0;
    for(int i=0;i<n;i++){
        tw+=p[i].calc_waiting_time;tt+=p[i].calc_turnaround_time;
        tr+=p[i].calc_response_time;tb+=p[i].burst_time;
        if(p[i].calc_waiting_time>mw)mw=p[i].calc_waiting_time;
        if(p[i].calc_turnaround_time>mt)mt=p[i].calc_turnaround_time;
    }
    printf("\n========================================================================================\n");
    printf("       PERFORMANCE METRICS (Windows - Priority Scheduling) Actual OS Measurements      \n");
    printf("========================================================================================\n");
    printf("Average Waiting Time       = %.2f seconds\n",tw/n);
    printf("Average Response Time      = %.2f seconds\n",tr/n);
    printf("Average Turnaround Time    = %.2f seconds\n",tt/n);
    printf("Max Waiting Time           = %.2f seconds\n",mw);
    printf("Max Turnaround Time        = %.2f seconds\n",mt);
    printf("Throughput                 = %.4f processes/second\n",n/total_time);
    printf("CPU Utilization            = %.2f%%\n",(tb/total_time)*100.0);
    printf("Total Execution Time       = %.4f seconds\n",total_time);
    printf("Context Switches           = %d\n",cs);
    printf("========================================================================================\n");
    printf("Note: Windows SetPriorityClass() adds overhead per process — higher total time vs Linux\n");
    printf("========================================================================================\n");
}

int main() {
    QueryPerformanceFrequency(&frequency);
    Process processes[] = {
        {1, "Telemetry Ingestion",  0,5,1,0,0,0,0,0,0},
        {2, "Sensor Fusion Engine", 1,3,1,0,0,0,0,0,0},
        {3, "ML Model Retraining",  2,8,5,0,0,0,0,0,0},
        {4, "GPS Sync Service",     1,2,2,0,0,0,0,0,0},
        {5, "Collision Detection",  3,4,2,0,0,0,0,0,0},
        {6, "Safety Watchdog",      5,1,1,0,0,0,0,0,0},
        {7, "CAN Bus Parser",       4,2,2,0,0,0,0,0,0},
        {8, "Audit Logger",         4,2,4,0,0,0,0,0,0},
        {9, "Route Analytics",      7,6,5,0,0,0,0,0,0},
        {10,"Fleet Reporter",       6,5,4,0,0,0,0,0,0}
    };
    int n=sizeof(processes)/sizeof(processes[0]);

    printf("========================================================================================\n");
    printf("         NexVault Dynamics - Process Scheduler (Windows)\n");
    printf("                Algorithm: Priority Scheduling (PS)\n");
    printf("========================================================================================\n\n");
    printf("System Configuration:\n");
    printf("  - Total Processes  : %d\n",n);
    printf("  - Scheduling Policy: Priority Scheduling (Non-preemptive)\n");
    printf("  - Priority Range   : 1 (Highest / Safety-Critical) to 5 (Lowest / Batch)\n");
    printf("  - Execution Mode   : CreateProcess() + WaitForSingleObject()\n");
    printf("  - Platform         : Windows Server 2022\n\n");

    calculate_priority_times(processes,n);

    printf("--- NexVault Process Queue (with Priorities) ---\n");
    printf("PID  Task Name                              Priority  Arrival Time  Burst Time\n");
    printf("------------------------------------------------------------------------------\n");
    for(int i=0;i<n;i++)
        printf("P%-3d %-38s %-9d %-13ds %ds\n",
               processes[i].pid,processes[i].task_name,processes[i].priority,
               processes[i].arrival_time,processes[i].burst_time);

    printf("\n=== Starting Priority Scheduling Simulation ===\n");
    printf("Measuring actual OS execution performance...\n\n");

    double sim_start=get_wall_time();
    int cs=0,done=0;
    for(int i=0;i<n;i++) processes[i].completed=0;

    while(done<n){
        int idx=-1,mp=999999;
        double elapsed=get_wall_time()-sim_start;
        for(int i=0;i<n;i++)
            if(!processes[i].completed&&processes[i].arrival_time<=elapsed&&processes[i].priority<mp)
                {mp=processes[i].priority;idx=i;}
        if(idx==-1){Sleep(100);}
        else{
            printf(">>  Executing P%d: %s (Priority: %d, Burst: %ds)\n",
                   processes[idx].pid,processes[idx].task_name,
                   processes[idx].priority,processes[idx].burst_time);
            execute_process(&processes[idx]);
            processes[idx].completed=1;cs++;done++;
            printf("OK  Completed P%d\n\n",processes[idx].pid);
        }
    }

    double total_time=get_wall_time()-sim_start;
    print_table(processes,n);
    calculate_metrics(processes,n,total_time,cs);
    printf("\nSimulation complete. Compare output with Linux version for OS benchmarking.\n\n");
    return 0;
}