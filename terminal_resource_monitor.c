// macOS Terminal Resource Monitor
// Montors systems CPU, MEMORY, and DISK Usage, Prints out Running Tasks by ID
// Created by Specter

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/task_info.h> 
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/utsname.h> 
#include <sys/types.h>
#include <time.h>
#include <libproc.h>

//Gets the CPU usage
void get_cpu_usage() {
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_cpu_load_info_data_t cpu_load;
    
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load, &count) == KERN_SUCCESS) {
        unsigned long long user = cpu_load.cpu_ticks[CPU_STATE_USER];
        unsigned long long system = cpu_load.cpu_ticks[CPU_STATE_SYSTEM];
        unsigned long long idle = cpu_load.cpu_ticks[CPU_STATE_IDLE];
        
        printf("CPU Ticks - User: %llu, System: %llu, Idle: %llu\n", user, system, idle);
    } else {
        perror("Failed to fetch CPU stats");
    }
}

//Gets the CPU Usage and prints it out in a percentage bar format
void get_visual_cpu_usage() {
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    host_cpu_load_info_data_t cpu_load;
    
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load, &count) == KERN_SUCCESS) {
        unsigned long long user = cpu_load.cpu_ticks[CPU_STATE_USER];
        unsigned long long system = cpu_load.cpu_ticks[CPU_STATE_SYSTEM];
        unsigned long long idle = cpu_load.cpu_ticks[CPU_STATE_IDLE];

        double total_ticks = user + system + idle;
        if (total_ticks == 0) {
            printf("CPU Usage: 0%% [          ]\r");
            fflush(stdout);
            return;
        }

        double user_percent = (user / total_ticks) * 100;
        double system_percent = (system / total_ticks) * 100;

        int bar_width = 70; // Width of the progress bar
        int user_bar = (int)(user_percent / 100.0 * bar_width);
        int system_bar = (int)(system_percent / 100.0 * bar_width);

        printf("CPU Usage: %.2f%% [", user_percent + system_percent);
        for (int i = 0; i < bar_width; ++i) {
            if (i < user_bar)
                putchar('#');
            else if (i < user_bar + system_bar)
                putchar('=');
            else
                putchar(' ');
        }
        printf("]\r");
        fflush(stdout);
    } else {
        perror("Failed to fetch CPU stats");
    }
}

//Gets the Memory Usage
void get_memory_usage() {
    vm_size_t page_size;
    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm_stats;

    host_page_size(mach_port, &page_size);

    if (host_statistics64(mach_port, HOST_VM_INFO64, (host_info_t)&vm_stats, &count) == KERN_SUCCESS) {
        long long free_mem = (int64_t)vm_stats.free_count * page_size;
        long long active_mem = (int64_t)vm_stats.active_count * page_size;
        long long inactive_mem = (int64_t)vm_stats.inactive_count * page_size;
        long long wire_mem = (int64_t)vm_stats.wire_count * page_size;
        
        long long used_mem = active_mem + wire_mem + inactive_mem;

        printf("Memory - Used: %lld MB, Free: %lld MB\n", used_mem / 1024 / 1024, free_mem / 1024 / 1024);
    } else {
        perror("Failed to fetch VM stats");
    }
}

//Gets the Memory usage and prints it out in a Percentage bar format
void get_visual_memory_usage() {
    vm_size_t page_size;
    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm_stats;

    host_page_size(mach_port, &page_size);

    if (host_statistics64(mach_port, HOST_VM_INFO64, (host_info_t)&vm_stats, &count) == KERN_SUCCESS) {
        long long total_mem = (int64_t)(vm_stats.active_count + vm_stats.inactive_count + vm_stats.wire_count + vm_stats.free_count) * page_size;

        long long free_mem = (int64_t)vm_stats.free_count * page_size;
        long long active_mem = (int64_t)vm_stats.active_count * page_size;
        long long inactive_mem = (int64_t)vm_stats.inactive_count * page_size;
        long long wire_mem = (int64_t)vm_stats.wire_count * page_size;

        double used_mem_percentage = ((total_mem - free_mem) / (double)total_mem) * 100.0;
        double free_mem_percentage = (free_mem / (double)total_mem) * 100.0;

        int bar_width = 50; // Width of the bar in characters
        int used_chars = (int)(bar_width * (used_mem_percentage / 100.0));
        int free_chars = bar_width - used_chars;

        printf("Memory - Used: %.2f%%, Free: %.2f%%\n", used_mem_percentage, free_mem_percentage);

        // Print the bar chart
        printf("Used: [");
        for (int i = 0; i < used_chars; ++i) {
            printf("#");
        }
        for (int i = 0; i < free_chars; ++i) {
            printf(" ");
        }
        printf("]\n");

        printf("Free: [");
        for (int i = 0; i < free_chars; ++i) {
            printf("#");
        }
        for (int i = 0; i < used_chars; ++i) {
            printf(" ");
        }
        printf("]\n");
    } else {
        perror("Failed to fetch VM stats");
    }
}

// 1. Create a struct to hold our process data so we can sort it later
typedef struct {
    pid_t pid;
    char name[256];
    uint64_t memory_bytes;
} ProcessInfo;

// 2. Comparison function for qsort (sorts descending by memory)
int compare_memory(const void *a, const void *b) {
    ProcessInfo *p1 = (ProcessInfo *)a;
    ProcessInfo *p2 = (ProcessInfo *)b;
    
    if (p1->memory_bytes < p2->memory_bytes) return 1;
    if (p1->memory_bytes > p2->memory_bytes) return -1;
    return 0;
}

//Gets the List of of Top Memory Intensive Tasks
void list_top_memory_tasks() {
    // 3. Get the list of all PIDs
    int buffer_size = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (buffer_size <= 0) {
        fprintf(stderr, "Failed to get process list size\n");
        return;
    }

    pid_t *pids = (pid_t *)malloc(buffer_size);
    if (pids == NULL) {
        fprintf(stderr, "Memory allocation failed for PIDs\n");
        return;
    }

    int bytes_returned = proc_listpids(PROC_ALL_PIDS, 0, pids, buffer_size);
    int num_pids = bytes_returned / sizeof(pid_t);

    // 4. Allocate an array to hold the detailed info for each process
    ProcessInfo *procs = (ProcessInfo *)malloc(num_pids * sizeof(ProcessInfo));
    if (procs == NULL) {
        fprintf(stderr, "Memory allocation failed for ProcessInfo\n");
        free(pids);
        return;
    }

    int valid_count = 0;

    // 5. Iterate through PIDs, fetch name and memory, and store in the array
    for (int i = 0; i < num_pids; i++) {
        pid_t pid = pids[i];
        if (pid == 0) continue; // Skip kernel_task baseline

        ProcessInfo info;
        info.pid = pid;
        info.memory_bytes = 0;

        // Fetch process name
        int name_ret = proc_name(pid, info.name, sizeof(info.name));
        if (name_ret <= 0) {
            strcpy(info.name, "[Restricted / Unknown]");
        }

        // Fetch memory usage (Resident Set Size)
        struct proc_taskinfo tinfo;
        int info_ret = proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &tinfo, sizeof(tinfo));
        
        // proc_pidinfo returns the size of the data written on success
        if (info_ret == sizeof(tinfo)) {
            info.memory_bytes = tinfo.pti_resident_size;
        }

        // Store it in our array
        procs[valid_count] = info;
        valid_count++;
    }

    // 6. Sort the array of structs by memory_bytes (Descending)
    qsort(procs, valid_count, sizeof(ProcessInfo), compare_memory);

    // 7. Print the Top 5
    printf("\nTOP 5 MEMORY USAGE APPLICATIONS\n");
    printf("%-10s %-30s %s\n", "PID", "PROCESS NAME", "MEMORY (MB)");
    printf("----------------------------------------------------------\n");

    int limit = (valid_count < 5) ? valid_count : 5;
    for (int i = 0; i < limit; i++) {
        // Convert bytes to Megabytes for easier reading
        double memory_mb = procs[i].memory_bytes / (1024.0 * 1024.0);
        printf("%-10d %-30s %.2f MB\n", procs[i].pid, procs[i].name, memory_mb);
    }

    // 8. Clean up
    free(pids);
    free(procs);
}

//Gets the Disk Usage for Disks
void get_disk_usage() {
    struct statfs stats;
    
    if (statfs("/", &stats) == 0) {
        uint64_t total_space = stats.f_blocks * stats.f_bsize;
        uint64_t free_space = stats.f_bfree * stats.f_bsize;
        uint64_t used_space = total_space - free_space;

        printf("Disk '/' - Total: %llu GB, Used: %llu GB, Free: %llu GB\n", 
               total_space / 1024 / 1024 / 1024, 
               used_space / 1024 / 1024 / 1024, 
               free_space / 1024 / 1024 / 1024);
    } else {
        perror("Failed to fetch disk stats");
    }
}

//Gets the system up time and formats it for human readability
void get_system_uptime() {
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval boottime;
    size_t len = sizeof(boottime);

    // Fetch the kernel boot time using sysctl
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == -1) {
        perror("sysctl");
        return;
    }

    // Get the current time
    struct timeval now;
    gettimeofday(&now, NULL);

    // Calculate the uptime in seconds
    int uptime_seconds = difftime(now.tv_sec, boottime.tv_sec);

    // Convert uptime to days, hours, minutes and seconds
    int uptime_days = (int)uptime_seconds / 86400;
    uptime_seconds %= 86400;
    int uptime_hours = (int)uptime_seconds / 3600;
    uptime_seconds %= 3600;
    int uptime_minutes = (int)uptime_seconds / 60;
    double uptime_seconds_remaining = uptime_seconds % 60;

    // Print the uptime
    printf("System Uptime: %d days, %d hours, %d minutes, %.2f seconds\n",
           uptime_days, uptime_hours, uptime_minutes, uptime_seconds_remaining);
}

//Gets the lists of ALL Running Tasks and prints them out by ID
void list_running_tasks() {
    // 1. Get the buffer size required to hold all active PIDs
    // Passing NULL and 0 returns the required byte size
    int buffer_size = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (buffer_size <= 0) {
        fprintf(stderr, "Failed to get process list size\n");
        return;
    }

    // 2. Allocate memory for the PIDs array
    pid_t *pids = (pid_t *)malloc(buffer_size);
    if (pids == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    // 3. Fetch the PIDs into the allocated buffer
    // The function returns the number of bytes written
    int bytes_returned = proc_listpids(PROC_ALL_PIDS, 0, pids, buffer_size);
    int num_pids = bytes_returned / sizeof(pid_t);

    if (num_pids <= 0) {
        fprintf(stderr, "Failed to fetch PIDs\n");
        free(pids);
        return;
    }

    // 4. Print the header
    printf("%-10s %s\n", "PID", "PROCESS NAME");
    printf("----------------------------------------\n");

    // 5. Iterate through the PIDs and get their names
    for (int i = 0; i < num_pids; i++) {
        pid_t pid = pids[i];
        
        // Skip PID 0 if you don't want to list the kernel_task
        if (pid == 0) continue; 

        char name_buffer[256]; // Buffer to hold the process name
        
        // Fetch the name of the process
        int ret = proc_name(pid, name_buffer, sizeof(name_buffer));

        if (ret > 0) {
            printf("%-10d %s\n", pid, name_buffer);
        } else {
            // macOS restricts reading details of system or other users' processes 
            // if you aren't running as root.
            printf("%-10d %s\n", pid, "[Restricted / Unknown]");
        }
    }

    // 6. Clean up
    free(pids);
}


//The Main Program that calls and formats the resource monitoring functions
int main() {
    system("clear");
    printf("Starting macOS Resource Monitor...\n\n");
    while (1) {
        system("clear");
        printf("---------------------------------------------\n");
        printf("Specter's macOS Terminal Resource Monitor \n");
        printf("---------------------------------------------\n");
        printf("CPU------------------------------------------\n");
        //get_cpu_usage();
        get_visual_cpu_usage();
        printf("---------------------------------------------\n");
        printf("MEMORY---------------------------------------\n");
        get_memory_usage();
        get_visual_memory_usage();
        list_top_memory_tasks();
        printf("---------------------------------------------\n");
        printf("DISK-----------------------------------------\n");
        get_disk_usage();
        printf("---------------------------------------------\n");
        printf("SYSTEM UP TIME-------------------------------\n");
        get_system_uptime();
        printf("---------------------------------------------\n");
        printf("TASKS----------------------------------------\n");
        list_running_tasks(); // Running Tasks
        printf("---------------------------------------------\n");
        sleep(2);
        //This Clears the screen so it doesnt SPAM the Terminal
        system("clear");
    }
    return 0;
}