#include <stdio.h>


long long int fact(int n) {
    if (n == 0 || n == 1) {
        return 1;
    }
    return n * fact(n - 1); //Recurssion inner most is returned first. 23/10/2026
}

int main() {
    int a = 1;
    int choice;

    while (a) {
        printf("\n1. Calculate Factorial via Iteration\n");
        printf("2. Calculate Factorial via Recursion\n");
        printf("Choose an option: ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            break;
        }

        if (choice == 1 || choice == 2) {
            int n;
            printf("Enter a non-negative integer: ");
            scanf("%d", &n);

            if (n < 0) {
                printf("Factorial of a negative number doesn't exist.\n");
            } else {
                long long result = 1;

                if (choice == 1) {
                    
                    for (int i = 1; i <= n; i++) {
                        result *= i;
                    }
                    printf("Factorial of %d (By Iteration) = %lld\n", n, result);
                } else {
                    // Recursive Calculation
                    result = fact(n);
                    printf("Factorial of %d (By Recursion) = %lld\n", n, result);
                }
            }

            char conti;
            printf("Do you want to continue? (Y for Yes, N for No): ");
            scanf(" %c", &conti);

            if (conti == 'N' || conti == 'n') {
                a = 0;
            } else if (conti != 'Y' && conti != 'y') {
                printf("Invalid input. Exiting the program.\n");
                a = 0;
            }

        } else {
            printf("Invalid choice. Please choose 1 or 2.\n");
            while (getchar() != '\n');
        }
    }

    return 0;
}