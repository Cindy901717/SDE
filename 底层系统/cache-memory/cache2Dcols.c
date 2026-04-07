#define Global_row 3000 
#define Global_col 500

int arr2D [Global_row][Global_col];

int main(void){
	int i, v;

	for (i = 0; i <  Global_col; i++){
		for (v = 0; v < Global_row; v++){
			arr2D[v][i] = i + v;
		}
	}
	return 0;
}
