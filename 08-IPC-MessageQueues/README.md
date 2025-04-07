# POSIX Message Queue IPC Examples

## 1. Introduction

Welcome to this repository of examples demonstrating Inter-Process Communication (IPC) using **POSIX Message Queues** on Linux operating systems.

**What is IPC?** IPC is a set of mechanisms that allow independent processes to communicate and synchronize their actions.

**What are POSIX Message Queues (`<mqueue.h>`)?**
This is one of the IPC mechanisms standardized by POSIX. It provides a way for processes to exchange data in the form of *messages* through a kernel-managed *queue*. Key features include:

* **Kernel-Persistent:** The queue exists within the kernel even if no process currently has it open (until it's explicitly deleted or the system restarts).
* **Name-Based Access:** Processes access the queue via a unique name (e.g., `/my_queue_name`), typically mapped into the virtual filesystem at `/dev/mqueue/`.
* **Message Boundaries:** Unlike pipes or streams, message queues preserve the boundaries between messages. A single read (`mq_receive`) retrieves exactly one message that was sent (`mq_send`).
* **Priorities:** Each message can be assigned a priority level. By default, receiving from the queue returns the highest-priority message available.
* **Blocking/Non-Blocking:** Send and receive operations can be configured to be blocking (waiting) or non-blocking.

**Purpose of this Repository:** To provide practical C examples, ranging from basic to slightly more advanced, illustrating how to use POSIX message queues for various IPC tasks.

## 2. Prerequisites

* A Linux operating system (examples developed and tested on Linux).
* GCC (GNU Compiler Collection).
* Basic knowledge of C programming.
* Understanding of basic process management in Linux (`fork`, `waitpid`).

## 3. Directory Structure
```bash
    ├── Ex1
    │   ├── Makefile
    │   ├── Test
    │   ├── Test.c # Exercise 1: Simple Send/Receive
    │   └── Test.o
    ├── Ex2
    │   ├── Makefile
    │   ├── Test
    │   ├── Test.c # Exercise 2: Character Count (Bidirectional Comm)
    │   └── Test.o
    └── Ex3
        ├── Makefile
        ├── Test
        ├── Test.c # Exercise 3: Three-Process Communication (Priorities)
        └── Test.o    
```

## 4. Exercise Details

Each exercise resides in a subdirectory and contains the source code file `Test.c`.

### 4.1 Exercise 1: Simple Send/Receive (`Ex1/Test.c`)

* **Goal:** Demonstrate the most basic one-way communication from a parent process to a child process.
* **Approach:**
    1.  Uses **one** single message queue (e.g., `/my_test_queue`).
    2.  The parent process creates (`O_CREAT`) and opens the queue in read/write mode (`O_RDWR`).
    3.  The parent creates a child process using `fork()`.
    4.  The parent sends a simple string message using `mq_send()`.
    5.  The child opens the *existing* queue (no `O_CREAT` needed).
    6.  The child receives the message using `mq_receive()` (this function will block until a message is available).
    7.  The child prints the content of the received message.
    8.  The parent uses `waitpid()` to wait for the child to terminate.
    9.  The parent closes its queue descriptor using `mq_close()` and, **most importantly**, removes the queue name from the system using `mq_unlink()`.
* **Key Functions Used:** `mq_open`, `mq_send`, `mq_receive`, `fork`, `waitpid`, `mq_close`, `mq_unlink`.
* **Compilation:**
    ```bash
    gcc Ex1/Test.c -o Ex1/ex1 -lrt
    ```
* **Execution:**
    ```bash
    ./Ex1/ex1
    ```

### 4.2 Exercise 2: Character Count (`Ex2/Test.c`)

* **Goal:** Extend communication to be bidirectional to accomplish a task: parent sends a string, child counts characters and sends the result back to the parent.
* **Approach:**
    1.  Uses **two** separate message queues for clear separation of communication directions:
        * Queue 1 (`/p_to_c_string_queue`): Parent sends the original string to the child.
        * Queue 2 (`/c_to_p_string_queue`): Child sends the character count (as a string) back to the parent.
    2.  The parent creates and opens both queues.
    3.  The parent `fork()`s a child process.
    4.  The parent sends the original string via Queue 1.
    5.  The child receives the string from Queue 1.
    6.  The child counts the characters using `strlen()`.
    7.  The child converts the integer count to a string using `snprintf()`.
    8.  The child sends the result string (the count) back to the parent via Queue 2.
    9.  The parent receives the result string from Queue 2.
    10. The parent converts the result string back to an integer using `atoi()`.
    11. The parent prints the character count.
    12. The parent `waitpid()`s for the child to finish.
    13. The parent `mq_close()`s and `mq_unlink()`s **both** queues.
* **Key Functions Used:** `mq_open` (x2), `mq_send` (x2), `mq_receive` (x2), `fork`, `waitpid`, `mq_close` (x2), `mq_unlink` (x2), `strlen`, `snprintf`, `atoi`.
* **Compilation:**
    ```bash
    gcc Ex2/Test.c -o Ex2/ex2 -lrt
    ```
* **Execution:**
    ```bash
    ./Ex2/ex2
    ```

### 4.3 Exercise 3: Three-Process Communication (`Ex3/Test.c`)

* **Goal:** Coordinate work between three processes (Parent, Child 1, Child 2) using only **one** message queue. Child 1 must process a message from the Parent *before* Child 2 receives the processed message.
* **Approach:**
    1.  Uses **one** single message queue (e.g., `/common_queue`).
    2.  Leverages message **priorities** to control the flow:
        * Original message from Parent has a **low** priority (e.g., 10).
        * Processed (uppercase) message from Child 1 has a **high** priority (e.g., 20).
    3.  The parent creates the queue and `fork()`s two child processes (Child 1 and Child 2).
    4.  The parent sends the initial message with **low** priority (10).
    5.  Child 1 calls `mq_receive()`. Since the parent's message is the only one available (and thus the highest priority *currently* available), Child 1 receives it.
    6.  Child 1 converts the string to uppercase (`toupper()`).
    7.  Child 1 sends the processed string back to the **same queue** but with **high** priority (20).
    8.  Child 2 calls `mq_receive()`. Since Child 1's message (priority 20) has a higher priority than any other message (if any existed), Child 2 receives this processed message.
    9.  Child 2 prints the uppercase string.
    10. The parent `waitpid()`s for **both** children to terminate.
    11. The parent `mq_close()`s and `mq_unlink()`s the single queue.
* **Key Functions Used:** `mq_open`, `mq_send` (with priority argument), `mq_receive` (implicitly gets highest priority, can retrieve received priority), `fork` (x2), `waitpid` (x2), `mq_close`, `mq_unlink`, `toupper`.
* **Compilation:**
    ```bash
    gcc Ex3/Test.c -o Ex3/ex3 -lrt
    ```
* **Execution:**
    ```bash
    ./Ex3/ex3
    ```

## 5. Core Concepts Explained

* **POSIX Message Queues (`<mqueue.h>`)**: Provides the standard API for working with message queues. Queues are managed by the kernel, ensuring messages aren't lost if sender/receiver processes terminate unexpectedly (unless the queue is deleted). Queue names starting with `/` are typically created in the `/dev/mqueue/` virtual filesystem.

* **`mq_open()`**: Used to create a new (`O_CREAT`) or open an existing message queue.
    * `flags`: Specify access mode (`O_RDONLY`, `O_WRONLY`, `O_RDWR`) and behavior (e.g., `O_CREAT`, `O_EXCL`, `O_NONBLOCK`).
    * `mode`: Access permissions (e.g., `0666`) if creating a new queue.
    * `mq_attr`: Pointer to a `struct mq_attr` to set attributes when creating a new queue (most importantly `mq_maxmsg` - max number of messages, and `mq_msgsize` - max message size in bytes).

* **`mq_send()`**: Sends a message (as a byte array) to the queue.
    * Must specify the exact length of the message. The message size cannot exceed `mq_msgsize`.
    * By default, the function blocks if the queue is full.
    * The `msg_prio` argument allows assigning a priority to the message (higher numbers usually mean higher priority).

* **`mq_receive()`**: Receives a message from the queue.
    * By default, the function blocks if the queue is empty.
    * By default, it receives the highest-priority message among those waiting.
    * The provided buffer must be large enough (at least `mq_msgsize` of the queue).
    * Can retrieve the priority of the received message via the `msg_prio` pointer argument.
    * The function returns the actual number of bytes in the received message.

* **Message Priorities**: Allow processes to send messages with different levels of importance. `mq_receive` prioritizes retrieving the message with the highest priority value, enabling more complex workflows like in Exercise 3.

* **`mq_close()` vs `mq_unlink()`**: It's crucial to understand the difference:
    * `mq_close(mqd_t mqdes)`: Closes a *file descriptor* associated with the message queue *within the calling process*. The queue itself continues to exist in the kernel.
    * `mq_unlink(const char *name)`: Removes the *name* of the message queue from the system. The kernel will actually free the queue's resources when *all* processes have `mq_close()`d it (reference count drops to 0) **AND** it has been `mq_unlink()`ed. **Always call `mq_unlink()` when the queue is no longer needed** to prevent system resource leaks. Typically, the "owner" process (e.g., the parent) calls `mq_unlink()` after child processes have finished.

* **Process Management (`fork`, `waitpid`)**: Standard POSIX/Linux tools used to create new processes and manage their termination. `waitpid` is essential to ensure the parent process doesn't terminate and `mq_unlink` the queue before child processes are done using it.

* **Linking (`-lrt`)**: The POSIX message queue functions (and some other real-time functions) reside in the real-time library (`librt`). Therefore, the `-lrt` flag must be specified during compilation to link the program against this library.

## 6. Important Notes

* These examples require linking with the real-time library (`-lrt`) during compilation (`gcc ... -lrt`).
* Ensure you have the necessary permissions to create message queues in `/dev/mqueue`. If you encounter "Permission denied" errors, you might need to check system permissions or (only in development/testing environments) run with `sudo`, although this is generally not recommended for regular applications.
* If a program crashes before calling `mq_unlink()`, the message queue might persist in the system. You can view existing queues by listing the contents of the `/dev/mqueue/` directory and manually remove them if necessary (e.g., `sudo rm /dev/mqueue/your_queue_name`).
