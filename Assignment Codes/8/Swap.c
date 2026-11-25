#include <stdio.h>

void swap(int *px, int *py){
    int temp;
    temp= *px;
    *px= *py;
    *py= temp;
}

int main(){
    int a,b;
    int i=1;
    char ch;
    printf("\n===Welcome to pointer swapping===\n");
    while(i==1){
        printf("\nEnter the two numbers to be swapped:\n");
        printf("Enter the first number: ");
        while(scanf("%d", &a) !=1 ){
            printf("Invalid input! Please enter an integer: ");
            while(getchar() != '\n');
        }
        printf("Enter the second number: ");
        while(scanf("%d", &b) !=1 ){
            printf("Invalid input! Please enter an integer: ");
            while(getchar() != '\n');
        }

        printf("\n\nBefore swapping: a = %d, location = %p\n", a, (void*)&a);
        printf("Before swapping: b = %d, location = %p\n\n\n", b, (void*)&b);
        swap(&a,&b);
        printf("After swapping: a = %d, location = %p\n", a, (void*)&a);
        printf("After swapping: b = %d, location = %p\n", b, (void*)&b);

        printf("\nDo you want to swap again? (Y for Yes, N for No): ");
        while(scanf(" %c", &ch) !=1 ){
            printf("Invalid input! Please enter Y or N: ");
            while(getchar() != '\n');
        }
        if(ch == 'N' || ch == 'n'){
            i=0;
        }
    }
}