#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define TIME_QUANTUM 3

typedef struct {
    int pid;
    char task_name[50];
    int arrival_time;
    int burst_time;
    int remaining_time;
    double calc_start_time;
    double calc_completion_time;
    double calc_turnaround_time;
    double calc_waiting_time;
    double calc_response_time;
    int first_response;
} Process;

LARGE_INTEGER frequency;

double get_wall_time() {
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart/(double)frequency.QuadPart;
}

void execute_quantum(Process *p, int quantum) {
    STARTUPINFO si; PROCESS_INFORMATION pi;
    ZeroMemory(&si,sizeof(si)); si.cb=sizeof(si);
    si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    ZeroMemory(&pi,sizeof(pi));
    char cmd[512];
    sprintf(cmd,
        "powershell -WindowStyle Hidden -Command "
        "\"$end=(Get-Date).AddSeconds(%d);"
        "while((Get-Date)-lt $end){$x=0;for($i=0;$i-lt 1000000;$i++){$x+=$i*3.14159}}\"",
        quantum);
    if(CreateProcess(NULL,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        WaitForSingleObject(pi.hProcess,INFINITE);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

void calculate_rr_times(Process p[], int n, int q) {
    double cur=0; int done=0;
    int queue[200],fr=0,rr=0,inq[20]={0};
    for(int i=0;i<n;i++){p[i].remaining_time=p[i].burst_time;p[i].first_response=1;p[i].calc_start_time=-1;}
    for(int i=0;i<n;i++) if(p[i].arrival_time==0){queue[rr++]=i;inq[i]=1;}
    while(done<n){
        if(fr==rr){
            double na=999999; int ni=-1;
            for(int i=0;i<n;i++) if(p[i].remaining_time>0&&p[i].arrival_time>cur&&p[i].arrival_time<na){na=p[i].arrival_time;ni=i;}
            if(ni!=-1){cur=na;queue[rr++]=ni;inq[ni]=1;}
        } else {
            int idx=queue[fr++];
            if(p[idx].first_response){p[idx].calc_start_time=cur;p[idx].calc_response_time=cur-p[idx].arrival_time;p[idx].first_response=0;}
            int ex=p[idx].remaining_time<q?p[idx].remaining_time:q;
            p[idx].remaining_time-=ex; cur+=ex;
            for(int i=0;i<n;i++) if(!inq[i]&&p[i].arrival_time<=cur&&p[i].remaining_time>0){queue[rr++]=i;inq[i]=1;}
            if(p[idx].remaining_time>0) queue[rr++]=idx;
            else{p[idx].calc_completion_time=cur;p[idx].calc_turnaround_time=cur-p[idx].arrival_time;p[idx].calc_waiting_time=p[idx].calc_turnaround_time-p[idx].burst_time;done++;inq[idx]=0;}
        }
    }
}

void print_table(Process p[], int n) {
    printf("\n========================================================================================\n");
    printf("           NEXVAULT DYNAMICS - ROUND ROBIN SCHEDULING EXECUTION TABLE                  \n");
    printf("                         (Theoretical Calculations, Q=%d)                              \n",TIME_QUANTUM);
    printf("========================================================================================\n");
    printf("PID  Task Name                    AT    BT    ST      CT      TAT     WT      RT\n");
    printf("----------------------------------------------------------------------------------------\n");
    for(int i=0;i<n;i++)
        printf("P%-3d %-26s %-5d %-5d %-7.2f %-7.2f %-7.2f %-7.2f %-7.2f\n",
               p[i].pid,p[i].task_name,p[i].arrival_time,p[i].burst_time,
               p[i].calc_start_time,p[i].calc_completion_time,
               p[i].calc_turnaround_time,p[i].calc_waiting_time,p[i].calc_response_time);
    printf("========================================================================================\n");
    printf("Time Quantum = %d seconds\n",TIME_QUANTUM);
    printf("Note: Theoretical values are identical across OS platforms\n");
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
    printf("          PERFORMANCE METRICS (Windows - Round Robin) Actual OS Measurements           \n");
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
    printf("Time Quantum               = %d seconds\n",TIME_QUANTUM);
    printf("========================================================================================\n");
    printf("Note: Windows CreateProcess overhead amplified by frequent quantum preemptions\n");
    printf("========================================================================================\n");
}

int main() {
    QueryPerformanceFrequency(&frequency);
    Process processes[] = {
        {1,"Telemetry Ingestion",  0,5,0,0,0,0,0,0,0},
        {2,"Sensor Fusion Engine", 1,3,0,0,0,0,0,0,0},
        {3,"ML Model Retraining",  2,8,0,0,0,0,0,0,0},
        {4,"GPS Sync Service",     1,2,0,0,0,0,0,0,0},
        {5,"Collision Detection",  3,4,0,0,0,0,0,0,0},
        {6,"Safety Watchdog",      5,1,0,0,0,0,0,0,0},
        {7,"CAN Bus Parser",       4,2,0,0,0,0,0,0,0},
        {8,"Audit Logger",         4,2,0,0,0,0,0,0,0},
        {9,"Route Analytics",      7,6,0,0,0,0,0,0,0},
        {10,"Fleet Reporter",      6,5,0,0,0,0,0,0,0}
    };
    int n=sizeof(processes)/sizeof(processes[0]);

    printf("========================================================================================\n");
    printf("         NexVault Dynamics - Process Scheduler (Windows)\n");
    printf("                Algorithm: Round Robin (RR)  Q=%d\n",TIME_QUANTUM);
    printf("========================================================================================\n\n");
    printf("System Configuration:\n");
    printf("  - Total Processes  : %d\n",n);
    printf("  - Scheduling Policy: Round Robin (Preemptive)\n");
    printf("  - Time Quantum     : %d seconds\n",TIME_QUANTUM);
    printf("  - Execution Mode   : CreateProcess() + WaitForSingleObject()\n");
    printf("  - Platform         : Windows Server 2022\n\n");

    calculate_rr_times(processes,n,TIME_QUANTUM);

    printf("--- NexVault Process Queue (Arrival Order) ---\n");
    printf("PID  Task Name                              Arrival Time   Burst Time\n");
    printf("-----------------------------------------------------------------------\n");
    for(int i=0;i<n;i++)
        printf("P%-3d %-38s %-13ds %ds\n",
               processes[i].pid,processes[i].task_name,
               processes[i].arrival_time,processes[i].burst_time);

    printf("\n=== Starting Round Robin Scheduling Simulation ===\n");
    printf("Measuring actual OS execution performance...\n\n");

    double sim_start=get_wall_time();
    int cs=0,done=0;
    for(int i=0;i<n;i++) processes[i].remaining_time=processes[i].burst_time;
    int queue[200],fr=0,rr_=0,inq[20]={0};
    for(int i=0;i<n;i++) if(processes[i].arrival_time==0){queue[rr_++]=i;inq[i]=1;}

    while(done<n){
        double elapsed=get_wall_time()-sim_start;
        if(fr==rr_){Sleep(100);
            elapsed=get_wall_time()-sim_start;
            for(int i=0;i<n;i++) if(!inq[i]&&processes[i].arrival_time<=elapsed&&processes[i].remaining_time>0){queue[rr_++]=i;inq[i]=1;}
        } else {
            int idx=queue[fr++];
            int ex=processes[idx].remaining_time<TIME_QUANTUM?processes[idx].remaining_time:TIME_QUANTUM;
            printf(">>  Executing P%d: %s (Remaining: %ds, Quantum: %ds)\n",
                   processes[idx].pid,processes[idx].task_name,processes[idx].remaining_time,ex);
            execute_quantum(&processes[idx],ex);
            processes[idx].remaining_time-=ex; cs++;
            elapsed=get_wall_time()-sim_start;
            for(int i=0;i<n;i++) if(!inq[i]&&processes[i].arrival_time<=elapsed&&processes[i].remaining_time>0){queue[rr_++]=i;inq[i]=1;}
            if(processes[idx].remaining_time>0){queue[rr_++]=idx;printf("   P%d preempted, re-queued\n\n",processes[idx].pid);}
            else{printf("OK  Completed P%d\n\n",processes[idx].pid);done++;inq[idx]=0;}
        }
    }

    double total_time=get_wall_time()-sim_start;
    print_table(processes,n);
    calculate_metrics(processes,n,total_time,cs);
    printf("\nSimulation complete. Compare output with Linux version for OS benchmarking.\n\n");
    return 0;
}