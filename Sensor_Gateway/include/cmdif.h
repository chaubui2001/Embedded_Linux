#ifndef CMDIF_H
#define CMDIF_H

/* Structure for arguments */
typedef struct {
    const char *socket_path;
} cmdif_args_t;

/**
 * @brief Main function for the Command Interface thread.
 * Listens on a UNIX domain socket for commands ("status", "stats")
 * and sends back the corresponding information.
 * @param arg Pointer to cmdif_args_t (or NULL if using default path).
 * @return Always returns NULL.
 */
void *cmdif_run(void *arg);

/**
 * @brief Signals the Command Interface thread to stop gracefully.
 * (Implementation might involve closing the listening socket or using a flag/pipe).
 */
void cmdif_stop(void);

#endif /* CMDIF_H */