#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* For string manipulation functions like strncmp, strerror */
#include <unistd.h>     /* For POSIX API functions */
#include <errno.h>      /* For error handling using errno */
#include <stdbool.h>    /* For boolean type */

/* Include project-specific headers */
#include "sysmon.h"     /* Header file for system monitoring functions */
#include "logger.h"     /* Header file for logging functionality */

/* --- Static variables to store previous CPU state --- */
/* These variables are used to calculate CPU usage between successive calls */
static unsigned long long prev_total_time = 0;      /* Sum of all previous CPU times */
static unsigned long long prev_total_idle_time = 0; /* Sum of previous idle and iowait times */
static bool first_call_cpu = true;                  /* Flag to indicate the first call to get CPU usage */

/*
 * @brief Reads CPU time statistics from the first line of /proc/stat.
 *        This function extracts user, nice, system, idle, iowait, irq, and softirq times.
 *        It also calculates the total time based on these components.
 * @param user_time Pointer to store user time.
 * @param nice_time Pointer to store nice time.
 * @param system_time Pointer to store system time.
 * @param idle_time Pointer to store idle time.
 * @param iowait_time Pointer to store iowait time.
 * @param irq_time Pointer to store irq time.
 * @param softirq_time Pointer to store softirq time.
 * @param total_time Pointer to store the calculated total time.
 * @return 0 on success, -1 on error.
 */
static int read_cpu_times(unsigned long long *user_time, unsigned long long *nice_time,
                          unsigned long long *system_time, unsigned long long *idle_time,
                          unsigned long long *iowait_time, unsigned long long *irq_time,
                          unsigned long long *softirq_time, unsigned long long *total_time) {
    /* Open the /proc/stat file to read CPU statistics */
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        /* Log an error message if the file cannot be opened */
        log_message(LOG_LEVEL_ERROR, "Failed to open /proc/stat: %s", strerror(errno));
        return -1;
    }

    /* Read the first line containing CPU times */
    int items_read = fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu",
                            user_time, nice_time, system_time, idle_time,
                            iowait_time, irq_time, softirq_time);

    /* Close the file after reading */
    fclose(fp);

    /* Check if all required fields were successfully read */
    if (items_read < 7) {
        /* Log an error if the required fields are missing */
        log_message(LOG_LEVEL_ERROR, "Failed to parse required CPU times from /proc/stat (read %d items, expected at least 7)", items_read);
        /* Reset all output parameters to zero */
        *user_time = *nice_time = *system_time = *idle_time = *iowait_time = *irq_time = *softirq_time = 0;
        *total_time = 0;
        return -1;
    }

    /* Calculate the total time as the sum of all CPU time components */
    *total_time = *user_time + *nice_time + *system_time + *idle_time +
                  *iowait_time + *irq_time + *softirq_time;

    return 0; /* Return success */
}

/*
 * @brief Helper function to parse a specific key's value from /proc/meminfo.
 *        This function searches for a key (e.g., "MemTotal") and retrieves its value in kB.
 * @param key The key to search for in /proc/meminfo.
 * @return The value associated with the key in kB, or -1 on error.
 */
static long get_mem_value(const char *key) {
    /* Open the /proc/meminfo file to read memory statistics */
    FILE *fp = fopen("/proc/meminfo", "r");
    char line[256];
    long value = -1;

    if (!fp) {
        /* Log an error message if the file cannot be opened */
        log_message(LOG_LEVEL_ERROR, "Failed to open /proc/meminfo: %s", strerror(errno));
        return -1;
    }

    /* Read the file line by line to find the specified key */
    while (fgets(line, sizeof(line), fp)) {
        char key_from_file[64];
        long val_kb;

        /* Parse the key-value pair from the line */
        if (sscanf(line, "%63[^:]:%ld kB", key_from_file, &val_kb) == 2) {
            /* Check if the key matches the requested key */
            if (strncmp(key_from_file, key, strlen(key)) == 0 && key_from_file[strlen(key)] == '\0') {
                value = val_kb;
                break;
            }
        }
    }

    /* Close the file after reading */
    fclose(fp);

    /* Log a warning if the key was not found */
    if (value == -1) {
        if (strcmp(key, "MemAvailable") != 0) {
            log_message(LOG_LEVEL_WARNING, "Key '%s' not found or could not be parsed in /proc/meminfo", key);
        } else {
            log_message(LOG_LEVEL_DEBUG, "Key 'MemAvailable' not found in /proc/meminfo (using fallback calculation).");
        }
    }

    return value; /* Return the value or -1 on error */
}

/*
 * @brief Retrieves system statistics, including RAM and CPU usage.
 *        This function calculates RAM usage and CPU usage based on the current system state.
 * @param stats Pointer to a system_stats_t structure to store the retrieved statistics.
 * @return 0 on success, -1 on error.
 */
int sysmon_get_stats(system_stats_t *stats) {
    if (!stats) return -1; /* Return error if the stats pointer is NULL */

    /* --- RAM Usage Calculation --- */
    /* Retrieve memory statistics from /proc/meminfo */
    stats->ram_total_kb = get_mem_value("MemTotal");
    long mem_free = get_mem_value("MemFree");
    long buffers = get_mem_value("Buffers");
    long cached = get_mem_value("Cached");
    long mem_available = get_mem_value("MemAvailable");

    /* Calculate free and used memory */
    if (mem_available != -1) {
        stats->ram_free_kb = mem_available;
    } else {
        if (mem_free != -1 && buffers != -1 && cached != -1) {
            stats->ram_free_kb = mem_free + buffers + cached;
        } else {
            stats->ram_free_kb = -1;
        }
    }

    /* Calculate RAM usage percentage */
    if (stats->ram_total_kb > 0 && stats->ram_free_kb >= 0) {
        stats->ram_used_kb = stats->ram_total_kb - stats->ram_free_kb;
        stats->ram_usage_percent = ((double)stats->ram_used_kb / stats->ram_total_kb) * 100.0;
    } else {
        stats->ram_used_kb = -1;
        stats->ram_usage_percent = -1.0;
        log_message(LOG_LEVEL_WARNING, "Could not calculate RAM usage (Total: %ld kB, Free: %ld kB)", stats->ram_total_kb, stats->ram_free_kb);
    }

    /* --- CPU Usage Calculation --- */
    /* Local variables to store current CPU times */
    unsigned long long current_user_time, current_nice_time, current_system_time, current_idle_time;
    unsigned long long current_iowait_time, current_irq_time, current_softirq_time, current_total_time;
    unsigned long long total_diff, idle_diff, busy_diff;
    double cpu_usage = -1.0; /* Default to error/unavailable */

    /* Read current CPU times */
    if (read_cpu_times(&current_user_time, &current_nice_time, &current_system_time, &current_idle_time,
                       &current_iowait_time, &current_irq_time, &current_softirq_time, &current_total_time) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to read current CPU times for usage calculation.");
        stats->cpu_usage_percent = -1.0;
        return 0; /* Return 0 as RAM info might still be valid */
    }

    /* Calculate total idle time (idle + iowait) */
    unsigned long long current_total_idle_time = current_idle_time + current_iowait_time;

    /* Handle the first call to establish baseline values */
    if (first_call_cpu) {
        log_message(LOG_LEVEL_DEBUG, "CPU usage monitor: First call, storing initial values.");
        prev_total_time = current_total_time;
        prev_total_idle_time = current_total_idle_time;
        first_call_cpu = false;
        stats->cpu_usage_percent = 0.0; /* Report 0% usage for the first interval */
    } else {
        /* Calculate differences since the last call */
        if (current_total_time < prev_total_time || current_total_idle_time < prev_total_idle_time) {
            log_message(LOG_LEVEL_WARNING, "CPU time counter wrap-around detected or invalid previous data (PrevTotal: %llu, CurrTotal: %llu, PrevIdle: %llu, CurrIdle: %llu). Resetting state.",
                         prev_total_time, current_total_time, prev_total_idle_time, current_total_idle_time);
            first_call_cpu = true; /* Reset state */
            stats->cpu_usage_percent = -1.0; /* Error for this call */
        } else {
            total_diff = current_total_time - prev_total_time;
            idle_diff = current_total_idle_time - prev_total_idle_time;

            if (total_diff > 0) {
                busy_diff = total_diff - idle_diff;
                if (busy_diff > total_diff) busy_diff = total_diff; /* Clamp values */

                cpu_usage = ((double)busy_diff / (double)total_diff) * 100.0;
                stats->cpu_usage_percent = cpu_usage; /* Store calculated usage */
            } else {
                stats->cpu_usage_percent = 0.0; /* No change implies 0% usage */
                log_message(LOG_LEVEL_DEBUG, "CPU Usage: No difference in total CPU time between samples.");
            }
        }

        /* Update previous values for the next call */
        prev_total_time = current_total_time;
        prev_total_idle_time = current_total_idle_time;
    }

    return 0; /* Return success */
}