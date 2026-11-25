#include <stdio.h>


int i=0;
int main(){
    float num1, num2;
    printf("WELLCOME TO CALCULATOR!!!\nUse + for addition\nUse - for subtraction\nUse * for multiplication\nUse / for division\n");
    while(i!=1){
        
        
        printf("Enter choice of operation (+, -, *, /): ");
        char op;
        scanf(" %c", &op);
        if(op== '+' || op== '-' || op== '*' || op== '/'){
            printf("Enter the first number: ");
            scanf("%f", &num1);

            printf("Enter the second number: ");
            scanf("%f", &num2);

    
            switch (op)
            {
            case ('+'):
                printf("The sum is %.3f\n", num1 + num2);
                i=1;
                break;

            case ('-'):
                printf("The difference is %.3f\n", num1 - num2);
                i=1;
             break;

            case ('*'):
               printf("The product is %.2f\n", num1 * num2);
                i=1;
              break;

            case ('/'):
                if(num2==0){
                  printf("Invalid denominator!\n");
                  }
              else{
                    printf("The integer quotient is %f\n", num1 / num2);
                    i=1;}
                break;

        
            default:
                printf("Please enter a valid input!\n");
                break;
            }
            

    
      }
    }
    return 0;
}

