/*
 * send_signal.c
 * 
 * Program to send signals (SIGINT or SIGUSR1) to other processes.
 * Usage: send_signal -u <pid> to send SIGUSR1
 *        send_signal -i <pid> to send SIGINT
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/*
* print_usage - Displays usage instructions
*/
void print_usage() {
    printf("Usage: send_signal -u <pid> to send SIGUSR1\n");
    printf("       send_signal -i <pid> to send SIGINT\n");
}

int main(int argc, char *argv[]) {
    int target_pid;
    int signal_type;

    // Check for correct number of arguments
    if (argc != 3) {
        print_usage();
        return 1;
    }

    // Parse signal type argument
    if (strcmp(argv[1], "-u") == 0) {
        signal_type = SIGUSR1;
    } else if (strcmp(argv[1], "-i") == 0) {
        signal_type = SIGINT;
    } else {
        print_usage();
        return 1;
    }
    
    // Parse PID argument
    target_pid = atoi(argv[2]);
    if (target_pid <= 0) {
        fprintf(stderr, "Error: Invalid PID\n");
        print_usage();
        return 1;
    }

    // Send the signal to the target process
    if (kill(target_pid, signal_type) == -1) {
        perror("Error sending signal");
        return 1;
    }

    return 0;
}
