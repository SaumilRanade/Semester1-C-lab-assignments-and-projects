#include <stdio.h>

int i = 0;

void read_matrix(int x[10][10], int *rows, int *cols) {
    printf("Enter the number of rows: ");
    while (scanf("%d", rows) != 1 || *rows <= 0 || *rows > 10) {
        printf("Invalid input! Please enter a positive integer (1-10): ");
        while (getchar() != '\n');
    }

    printf("Enter the number of columns: ");
    while (scanf("%d", cols) != 1 || *cols <= 0 || *cols > 10) {
        printf("Invalid input! Please enter a positive integer (1-10): ");
        while (getchar() != '\n');
    }

    printf("Enter the elements of the matrix:\n");
    for (int r = 0; r < *rows; r++) {
        for (int c = 0; c < *cols; c++) {
            while (scanf("%d", &x[r][c]) != 1) {
                printf("Invalid number! Re-enter element [%d][%d]: ", r, c);
                while (getchar() != '\n');
            }
        }
    }

    printf("The entered matrix is:\n");
    for (int r = 0; r < *rows; r++) {
        for (int c = 0; c < *cols; c++) {
            printf("%d ", x[r][c]);
        }
        printf("\n");
    }
}

int main() {
    int a[10][10], b[10][10];
    int rows_a, cols_a, rows_b, cols_b;
    char correct;
    printf("WELCOME TO MATRIX CALCULATOR!!! SHIRISH SIR!!!\n");
    while (i != 1) {
        printf("Enter the first matrix:\n");
        read_matrix(a, &rows_a, &cols_a);
        
        printf("Enter the second matrix:\n");
        read_matrix(b, &rows_b, &cols_b);

        if (rows_a != rows_b || cols_a != cols_b) {
            printf("\nError: Matrices must have the same dimensions for addition/subtraction!\n");
            printf("Please re-enter both matrices.\n\n");
            continue;
        }

        int valid = 0;
        while (valid == 0) {
            printf("Do you wish to change the matrix? (Y for Yes, N for No): ");
            scanf(" %c", &correct);

            if (correct == 'N' || correct == 'n') {
                valid = 1;
                printf("Enter choice of operation (+, -): ");
                char op;
                scanf(" %c", &op);

                if (op == '+' || op == '-') {
                    switch (op) {
                        case ('+'):
                            printf("The sum of the two matrices is:\n");
                            for (int r = 0; r < rows_a; r++) {
                                for (int c = 0; c < cols_a; c++) {
                                    printf("%d ", a[r][c] + b[r][c]);
                                }
                                printf("\n");
                            }
                            i = 1;
                            break;

                        case ('-'):
                            printf("The difference of the two matrices is:\n");
                            for (int r = 0; r < rows_a; r++) {
                                for (int c = 0; c < cols_a; c++) {
                                    printf("%d ", a[r][c] - b[r][c]);
                                }
                                printf("\n");
                            }
                            i = 1;
                            break;
                    }
                } else {
                    printf("Invalid operation. Please enter + or -.\n");
                }
            } else if (correct == 'Y' || correct == 'y') {
                valid = 1;
                i = 0;
            } else {
                printf("Invalid input. Please enter Y or N.\n");
                while (getchar() != '\n');
                valid = 0;
            }
        }
    }
    return 0;
}



