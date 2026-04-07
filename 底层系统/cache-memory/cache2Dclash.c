#define GLOBAL_ROW 128
#define GLOBAL_COL 8
#define ITERATIONS 100

int arr2D [GLOBAL_ROW][GLOBAL_COL];

int main(void){
	int iter, row, col;
	for (iter = 0; iter < ITERATIONS; iter++) {
		for (row = 0; row < GLOBAL_ROW; row += 64) {
			for (col = 0; col < GLOBAL_COL; col ++){
				arr2D[row][col] = iter + row + col;
			}
		}
	}
	return 0;
}
