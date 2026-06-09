#ifndef PROCESS_READER_H
#define PROCESS_READER_H

#include <stdbool.h>

#define MAX_PROCESSES 1024

typedef struct {
    int pid;
    char name[256];
    double cpu_percent;
    double ram_mb;
    double real_mem_mb;
    double gpu_percent;
    double cache_mb;
    int uid;
} ProcessInfo;

typedef struct {
    int process_count;
    ProcessInfo processes[MAX_PROCESSES];
} ProcessList;

void process_reader_init(void);
void process_reader_update(ProcessList *list, bool demo_mode);
void process_reader_cleanup(void);

#endif /* PROCESS_READER_H */
