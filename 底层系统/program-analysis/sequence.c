#include<stdio.h>
#include<stdlib.h>

int linear_function(int m, int x, int b){
    return m * x + b;
}

void print_usage(){
printf("Usage: ./sequence n x0 m c\n");
printf("where: n is a non-zero positive number of values in the sequence,\n");
printf("       x0 is an integer and is the first value in the sequence,\n");
printf("       m is an integer and is used as a multiplier of the previous term in the sequence,\n");
printf("       c is an integer and is added to the (m*previous) term\n");
printf("       ==> where next value in sequence is computed as x1 = m * x0 + c\n");
}

int main(int argc, char *argv[]){
    // Check the num of parameters
    if (argc != 5){
        print_usage();
        return 1;
    }
    
    // Convert parameter to integer
    int n  = atoi(argv[1]);
    int x0 = atoi(argv[2]);
    int m = atoi(argv[3]);
    int c = atoi(argv[4]);

    // Check n needs to be positive
    if (n <= 0){
        printf("Error: n must be a positive non-zero integer\n");
        return 1;
    }

    // Create and print array
    int current_value;
    current_value = x0;

    // Print the first value
    printf("%d", current_value);

    // Create and print remaining datas
    for (int i = 1; i < n; i++) {
        // Calculate the next value: x_next = m * x_current + c
        current_value = linear_function(m, current_value, c);
        printf(",%d", current_value);
    } 

    printf("\n");

    return 0;
}
