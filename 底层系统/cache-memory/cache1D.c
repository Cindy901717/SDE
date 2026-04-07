#include <stdio.h>

#define GLOBAL_Length 100000

int arr[GLOBAL_Length];

int main(void){
	int i;

	for (i = 0; i < GLOBAL_Length; i++) {
		arr[i] = i;
	}
	return 0;
}



