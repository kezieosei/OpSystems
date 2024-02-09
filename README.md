# OpSystems

## Overview

This assignment involves implementing a solution to the Producer-Consumer problem in the context of a shared printer queue. The setup includes user processes acting as producers adding print requests to the queue, and consumer threads representing printers processing print jobs from the queue. The main process initializes the system, manages command-line parameters, and serves as an interactive command console.

## Features

- User processes act as producers, generating print requests with random sizes.
- Printer threads serve as consumers, processing print jobs from the global queue.
- Command-line parameters include the number of producer processes and printer threads.
- Random delays between print jobs are simulated using `nanosleep()` or `sleep()`.
- Semaphores are used for synchronization and mutual exclusion.
- Shared memory is utilized for inter-process communication.
- Graceful termination of threads is ensured using signal handlers.
- Two queue implementations: LIFO and FIFO.

## Instructions

1. **Producer Processes (User Processes):**
   - Each user can submit up to 20 print jobs.
   - Print job sizes are determined randomly (between 100-1000 bytes).

2. **Printer Threads (Consumers):**
   - Each printer thread processes one print job at a time.
   - Threads exit only after all print requests have been processed.

3. **Main Process:**
   - Reads command-line parameters.
   - Initializes the system, including the global print queue and semaphores.
   - Acts as an interactive command console.

4. **Synchronization and Semaphores:**
   - Three semaphores are used: for queue fullness, emptiness, and mutual exclusion.
   - Semaphores are initialized before starting processes/threads.

5. **Deallocating Resources:**
   - The main process deallocates the print queue and semaphores after all print jobs are completed.
   - Printer threads exit after all print requests have been processed.

6. **Reporting:**
   - Execution times are measured as a function of the number of users and printers.
   - Average waiting times for print jobs are reported for both LIFO and FIFO queue implementations.
   - Mention any non-working portions of the code in the report.

## Implementation

- Two queue implementations are provided: LIFO and FIFO.
- Shared memory is used for inter-process communication.
- Semaphores are employed for synchronization and mutual exclusion.

## Conclusion

This README provides an overview of the assignment requirements, implementation details, and instructions for usage. The solution addresses the Producer-Consumer problem within the context of a shared printer queue, utilizing semaphores for synchronization and shared memory for inter-process communication. The provided code aims to demonstrate proficiency in concurrent programming and system-level synchronization techniques.
