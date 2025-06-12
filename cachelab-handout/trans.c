/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/**
 * @brief Optimized transpose for 32x32 matrices
 * Uses 8x8 blocking to optimize cache performance
 * Cache: 1KB direct mapped, 32-byte blocks = 32 sets, 8 ints per block
 * @param M: number of columns
 * @param N: number of rows  
 * @param A: input matrix
 * @param B: output matrix
 */
void transpose_32x32(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    
    // Process matrix in 8x8 blocks
    for (i = 0; i < 32; i += 8) {
        for (j = 0; j < 32; j += 8) {
            // Transpose each 8x8 block
            for (k = i; k < i + 8; ++k) {
                // Read a full row from A (8 elements)
                a0 = A[k][j];
                a1 = A[k][j + 1];
                a2 = A[k][j + 2];
                a3 = A[k][j + 3];
                a4 = A[k][j + 4];
                a5 = A[k][j + 5];
                a6 = A[k][j + 6];
                a7 = A[k][j + 7];
                
                // Write to corresponding column positions in B
                B[j][k] = a0;
                B[j + 1][k] = a1;
                B[j + 2][k] = a2;
                B[j + 3][k] = a3;
                B[j + 4][k] = a4;
                B[j + 5][k] = a5;
                B[j + 6][k] = a6;
                B[j + 7][k] = a7;
            }
        }
    }
}

/**
 * @brief Optimized transpose for 64x64 matrices
 * Uses sophisticated 8x8 blocking with 4x4 sub-blocks to handle cache conflicts
 * @param M: number of columns
 * @param N: number of rows
 * @param A: input matrix  
 * @param B: output matrix
 */
void transpose_64x64(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    
    // Process matrix in 8x8 blocks
    for (i = 0; i < 64; i += 8) {
        for (j = 0; j < 64; j += 8) {
            // Step 1: Process upper-left and upper-right 4x4 sub-blocks
            // Read from A's upper half, write to B's upper half
            for (k = i; k < i + 4; ++k) {
                a0 = A[k][j];
                a1 = A[k][j + 1];
                a2 = A[k][j + 2];
                a3 = A[k][j + 3];
                a4 = A[k][j + 4];
                a5 = A[k][j + 5];
                a6 = A[k][j + 6];
                a7 = A[k][j + 7];
                
                // Upper-left 4x4: correct position
                B[j][k] = a0;
                B[j + 1][k] = a1;
                B[j + 2][k] = a2;
                B[j + 3][k] = a3;
                
                // Upper-right 4x4: temporarily store in wrong position
                B[j][k + 4] = a4;
                B[j + 1][k + 4] = a5;
                B[j + 2][k + 4] = a6;
                B[j + 3][k + 4] = a7;
            }
            
            // Step 2: Process lower-left and move misplaced upper-right
            for (k = j; k < j + 4; ++k) {
                // Read misplaced elements from upper-right
                a0 = B[k][i + 4];
                a1 = B[k][i + 5];
                a2 = B[k][i + 6];
                a3 = B[k][i + 7];
                
                // Read lower-left 4x4 from A
                a4 = A[i + 4][k];
                a5 = A[i + 5][k];
                a6 = A[i + 6][k];
                a7 = A[i + 7][k];
                
                // Place lower-left in correct position
                B[k][i + 4] = a4;
                B[k][i + 5] = a5;
                B[k][i + 6] = a6;
                B[k][i + 7] = a7;
                
                // Move misplaced upper-right to correct position
                B[k + 4][i] = a0;
                B[k + 4][i + 1] = a1;
                B[k + 4][i + 2] = a2;
                B[k + 4][i + 3] = a3;
            }
            
            // Step 3: Process lower-right 4x4
            for (k = i + 4; k < i + 8; ++k) {
                a4 = A[k][j + 4];
                a5 = A[k][j + 5];
                a6 = A[k][j + 6];
                a7 = A[k][j + 7];
                
                B[j + 4][k] = a4;
                B[j + 5][k] = a5;
                B[j + 6][k] = a6;
                B[j + 7][k] = a7;
            }
        }
    }
}

/**
 * @brief Optimized transpose for 61x67 matrices
 * Uses 16x16 blocking for irregular matrix sizes
 * @param M: number of columns
 * @param N: number of rows
 * @param A: input matrix
 * @param B: output matrix
 */
void transpose_61x67(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, t, s;
    
    // Use 16x16 blocking for better cache utilization
    for (i = 0; i < N; i += 16) {
        for (j = 0; j < M; j += 16) {
            // Process each block, handling boundary conditions
            for (t = i; t < i + 16 && t < N; ++t) {
                for (s = j; s < j + 16 && s < M; ++s) {
                    B[s][t] = A[t][s];
                }
            }
        }
    }
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32) {
        transpose_32x32(M, N, A, B);
    } else if (M == 64 && N == 64) {
        transpose_64x64(M, N, A, B);
    } else if (M == 61 && N == 67) {
        transpose_61x67(M, N, A, B);
    } else {
        // Fallback for other sizes - simple blocking
        int i, j, ii, jj, tmp;
        int block_size = 8;
        
        for (ii = 0; ii < N; ii += block_size) {
            for (jj = 0; jj < M; jj += block_size) {
                for (i = ii; i < ii + block_size && i < N; i++) {
                    for (j = jj; j < jj + block_size && j < M; j++) {
                        tmp = A[i][j];
                        B[j][i] = tmp;
                    }
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

