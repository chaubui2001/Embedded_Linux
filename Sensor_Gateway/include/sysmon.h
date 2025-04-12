#ifndef SYSMON_H
#define SYSMON_H

#include <stddef.h>

/* Structure to hold system resource info */
typedef struct {
    double cpu_usage_percent; /* Rough CPU usage */
    long ram_total_kb;        /* Total RAM in KB */
    long ram_free_kb;         /* Free RAM in KB */
    long ram_used_kb;         /* Used RAM in KB */
    double ram_usage_percent; /* RAM usage percentage */
} system_stats_t;

/**
 * @brief Gets current system statistics (CPU, RAM).
 * NOTE: CPU usage calculation might be basic and require refinement.
 * @param stats Pointer to a system_stats_t struct to fill.
 * @return 0 on success, -1 on failure.
 */
int sysmon_get_stats(system_stats_t *stats);

#endif /* SYSMON_H */