////////////////////////////////////////////////////////////////////////////////
// Main File:        my_magic_square.c
// This File:        my_magic_square.c
// Other Files:      None
// Semester:         CS 354 Lecture 001 Fall 2025
// Grade Group:      gg11
// Instructor:       
// 
// Author:           Xinyi Wu
// Email:            xwu573@wisc.edu
// CS Login:         cindy
//
//////////////////// REQUIRED -- OTHER SOURCES OF HELP ///////////////////////// 
// Persons:          None
//
// Online sources:   https://en.wikipedia.org/wiki/Siamese_method
//                   https://en.wikipedia.org/wiki/Magic_square
//
// AI chats:         None
//
///////////////////////////////////////////////////////////////////////////////
// Copyright 2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
// Used by permission, CS354 FALL 2025, Hina Mahmood
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure that represents a magic square
typedef struct {
	int size;           // dimension of the square
	int **magic_square; // ptr to 2D heap array that stores magic square values
} MagicSquare;

/* TODO:
 * Prompts the user for magic square's size, read size, and
 * check if it is an odd number >= 3 
 * If not valid size, display the required error message and exit
 *
 * return the valid number
 */
int getSize() {
	int size;	
	printf("Enter magic square's size (odd integer >= 3)\n");
	if (scanf("%d", &size) != 1) {
		printf("Error reading input. \n");
		exit(1);
	}

	if (size < 3) {
		printf("Magic suqure size must be >= 3. \n");
		exit(1);
	}

	if (size % 2 == 0) {
		printf("Magic squre size must be odd.\n");
		exit(1);
	}
	return size;   
} 

/* TODO:
 * Creates a magic square of size n on the heap
 *
 * May use the Siamese magic square algorithm or alternative
 * algorithm that produces a valid magic square 
 *
 * n - the number of rows and columns
 *
 * returns a pointer to the completed MagicSquare struct
 */
MagicSquare *generateMagicSquare(int n) {
	// Allocate memory for MagicSquare structure
	MagicSquare *ms = (MagicSquare *)malloc(sizeof(MagicSquare));
	if (ms == NULL) {
		printf("Error allocating memory for MagicSquare.\n");
		exit(1);
	}	

	ms->size = n;

	// Allocate array of row pointers
	ms->magic_square = (int **)malloc(n * sizeof(int *));
	if (ms->magic_square == NULL){
		printf("Error allocating memory for magic square rows.\n");
		free(ms);
		exit(1);
	}

	// Allocate for each row
	for (int i = 0; i < n; i++) {
		*(ms->magic_square + i) = (int *)malloc(n * sizeof(int));
		if (*(ms->magic_square + i) == NULL){ 
			printf("Error allocating memory for magic square columns.\n");
			// Free previously allocated rows
			for (int j = 0; j < i; j++) {
				free(*(ms->magic_square + j));
			}
			free(ms->magic_square);
			free(ms);
			exit(1);
		} 
		// Initialize row to 0
		for (int j = 0; j < n; j++) {
			*(*(ms->magic_square + i)+j) = 0;
		}
	}

	// Simaese algorithm: start at middle of top row
	int row = 0;
	int col = n/2;

	// Place number 1 to n*n
	for (int num = 1; num <= n * n; num++) {
		// Place current number
		*(*(ms->magic_square + row) + col) = num;

		// Calculate next position (up and right)
		int next_row = (row - 1 + n) % n;
		int next_col = (col + 1) % n;

		// Check if next position is occupied
		if (*(*(ms->magic_square + next_row) + next_col) != 0) {
			// If occupied, move down from current position
			next_row = (row + 1) % n;
			next_col = col;
		}

		// Update position for next iteration
		row = next_row;
		col = next_col;
	}

	return ms;    
} 

/* TODO:  
 * Open a new file (or overwrite the existing file)
 * and write magic square values to the file
 * in a format specified in the assignment.
 *
 * See assignment for required file format.
 *
 * magic_square - the magic square to write to a file
 * filename - the name of the output file
 */
void fileOutputMagicSquare(MagicSquare *magic_square, char *filename) {
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		printf("Error opening file for writing.\n");
		exit(1);
	}

	int n = magic_square->size;

	// Write size on first line
	fprintf(fp, "%d\n", n);

	// Write each row
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			fprintf(fp, "%d", *(*(magic_square->magic_square + i) + j));
			if (j < n - 1) {
				fprintf(fp, ",");
			}
		}
		fprintf(fp, "\n");
	}

	if (fclose(fp) != 0) {
		printf("Error closing file.\n");
		exit(1);
	}
}


/* TODO:
 * Calls other functions to generate a magic square 
 * of the user-specified size and outputs the
 * created square to the output filename.
 * 
 * Add description of required CLAs here
 */
int main(int argc, char **argv) {
	// TODO: Check input arguments to get output filename
	if (argc != 2) {
		printf("Usage: ./my_magic_square <output_filename>\n");
		exit(1);
	}

	char *filename = *(argv + 1);

	// TODO: Get magic square's size from user
	int size = getSize();

	// TODO: Generate a magic square by correctly interpreting
	MagicSquare *ms = generateMagicSquare(size);

	//       the algorithm(s) in the write-up or by writing on your own.  
	//       You must confirm that your program produces a 
	//       valid Magic Square. See the provided Wikipedia page link for
	//       description.

	// TODO: Output the magic square
	fileOutputMagicSquare(ms, filename);

	// Free allocated memory
	for (int i = 0; i < ms->size; i++) {
		free(*(ms->magic_square + i));
	}
	free(ms->magic_square);
	free(ms);

	return 0;
} 

// 202509


