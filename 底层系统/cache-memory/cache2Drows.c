#include <stdio.h>

#define Global_rows 3000
#define Global_columns 500

int arr2D [Global_rows][Global_columns];

int main(void){
	int i, v;

	for (i=0; i < Global_rows; i++){
		for (v=0; v < Global_columns; v++){
			arr2D[i][v] = i + v;
		}
	}
	return 0;
}
