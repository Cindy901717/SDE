/*
 * my_div0_handler.c
 * 
 * Program to demonstrate handling of SIGFPE (divide by zero) and SIGINT signals.
 * Continuously prompts for two integers and performs division operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

// Global variable
int division_count = 0;

/*
 * sigfpe_handler - Handler for SIGFPE signal (divide by zero)
 * Prints error message, division count, and exits gracefully
 */
void sigfpe_handler(int sig) {
    printf("Error: a division by 0 operation was attempted.\n");
    printf("Total number of operations completed successfully: %d\n", division_count);
    printf("The program will be terminated.\n");
    exit(0);
}

/*
 * sigint_handler - Handler for SIGINT signal (Ctrl-C)
 * Prints division count and exits gracefully
 */
void sigint_handler(int sig) {
    printf("\nTotal number of operations completed successfully: %d\n", division_count);
    printf("The program will be terminated.\n");
    exit(0);
}

int main() {
    struct sigaction sigfpe_act;
    struct sigaction sigint_act;
    char buffer[100];
    int num1, num2;
    int quotient, remainder;

    // Setup SIGFPE handler (divide by zero)
    memset(&sigfpe_act, 0, sizeof(sigfpe_act));
    sigfpe_act.sa_handler = sigfpe_handler;
    if (sigaction(SIGFPE, &sigfpe_act, NULL) == -1) {
        fprintf(stderr, "Error: Failed to register SIGFPE handler\n");
        exit(1);
    }
    
    // Setup SIGINT handler (Ctrl-C)
    memset(&sigint_act, 0, sizeof(sigint_act));
    sigint_act.sa_handler = sigint_handler;
    if (sigaction(SIGINT, &sigint_act, NULL) == -1) {
        fprintf(stderr, "Error: Failed to register SIGINT handler\n");
        exit(1);
    }

    // Infinite loop for division operations
    while (1) {
        printf("Enter first integer: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        num1 = atoi(buffer);

        printf("Enter second integer: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        num2 = atoi(buffer);

        quotient = num1 / num2;
        remainder = num1 % num2;

        printf("%d / %d is %d with a remainder of %d\n", 
               num1, num2, quotient, remainder);

        division_count++;
    }

    return 0;
}

