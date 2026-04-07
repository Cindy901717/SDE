/*
* my_c_signal_handler.c
*
* Program to demonstrate signal handling with SIGALRM, SIGUSR1, and SIGINT.
* Sets up periodic alarms every 3 seconds and handles user-defined signals.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Global varibales
int alarm_seconds = 3;
int sigusr1_count = 0;

/*
* alarm_handler - Handler for SIGALRM signal
* Prints the process ID and current time, then rearms the alarm.
*/
void alarm_handler(int sig) {
    pid_t pid;
    time_t current_time;
    char *time_string;

    // Get process ID
    pid = getpid();

    // Get current time
    current_time = time(NULL);
    if (current_time == ((time_t) -1)) {
        fprintf(stderr, "Error: Failed to get current time\n");
        return;
    }

    // Convert time to string format
    time_string = ctime(&current_time);
    if (time_string == NULL) {
        fprintf(stderr, "Error: Failed to convert time to string\n");
        return;
    }

    // Remove newline from time string
    time_string[strlen(time_string) -1] = '\0';

    // Print PID and current time
    printf("PID: %d CURRENT TIME: %s\n", pid, time_string);

    // Rearm the alarm for another interval
    alarm(alarm_seconds);
}

/*
 * sigusr1_handler - Handler for SIGUSR1 signal
 * Prints a message and increments the counter
 */
void sigusr1_handler(int sig) {
    printf("Received SIGUSR1, user signal 1 counted.\n");
    sigusr1_count++;
}

/*
* sigint_handler - Handler for SIGINT signal (Ctrl-C)
* Prints the count of SIGUSR1 signals received and exits
*/
void sigint_handler(int sig) {
    printf("\nSIGNIT handled.\n");
    printf("SIGUSR1 was handled %d times. Exiting now.\n", sigusr1_count);
    exit(0);
}

int main() {
    struct sigaction alarm_act;
    struct sigaction sigusr1_act;
    struct sigaction sigint_act;

    // Setup SIGALRM handler
    memset(&alarm_act, 0, sizeof(alarm_act));
    alarm_act.sa_handler = alarm_handler;
    if (sigaction(SIGALRM, &alarm_act, NULL) == -1) {
        fprintf(stderr, "Error: Failed to register SIGALRM handler\n");
        exit(1);
    }

    // Setup SIGUSR1 handler
    memset(&sigusr1_act, 0, sizeof(sigusr1_act));
    sigusr1_act.sa_handler = sigusr1_handler;
    if (sigaction(SIGUSR1, &sigusr1_act, NULL) == -1) {
        fprintf(stderr, "Error: Failed to register SIGUSR1 handler\n");
        exit(1);
    }

     // Setup SIGINT handler
    memset(&sigint_act, 0, sizeof(sigint_act));
    sigint_act.sa_handler = sigint_handler;
    if (sigaction(SIGINT, &sigint_act, NULL) == -1) {
        fprintf(stderr, "Error: Failed to register SIGINT handler\n");
        exit(1);
    }

    // Set initial alarm
    alarm(alarm_seconds);

    // Print instructions
    printf("PID and current time: prints every 3 seconds.\n");
    printf("Type Ctrl-C to end the program.\n");

    // Infinite loop - signal handlers will interrupt this
    while (1) {
        
    }

    return 0;
}



