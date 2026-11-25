//Include nested structures

#include <stdio.h>

struct marks{
    float sub1;
    float sub2;
};

struct Stu {
    int roll;
    char name[50];     
    float marks[3];    
    float tot, per;
};


int getInt(const char* prompt, const char* errMsg) {
    int val;
    printf("%s", prompt);
    while (scanf("%d", &val) != 1 || val <= 0) {
        printf("%s", errMsg);
        while (getchar() != '\n'); 
    }
    return val;
}


float getFloat(const char* prompt, const char* errMsg) {
    float val;
    printf("%s", prompt);
    while (scanf("%f", &val) != 1 || val < 0 || val > 100) {
        printf("%s", errMsg);
        while (getchar() != '\n'); 
    }
    return val;
}

int main() {
    int n, i, j;

    printf("=== Student Records ===\n");
    n = getInt("Number of students: ", "Invalid! Enter a positive integer: ");

    struct Stu s[n]; 


    for (i = 0; i < n; i++) {
        printf("\nStudent %d Details:\n", i + 1);
        
        s[i].roll = getInt("Roll No: ", "Invalid roll number! Enter a positive integer: ");
        
        // Clear buff
        while (getchar() != '\n'); 
        
        printf("Name: ");
        scanf("%[^\n]", s[i].name);
        
        printf("Enter marks for 3 subjects:\n");
        s[i].tot = 0; 
        for (j = 0; j < 3; j++) {
            char markPrompt[50];
            sprintf(markPrompt, " Subject %d: ", j + 1);
            s[i].marks[j] = getFloat(markPrompt, " Invalid marks! Enter a value between 0 and 100: ");
            s[i].tot += s[i].marks[j];
        }
        
        s[i].per = (s[i].tot / 300.0) * 100.0;
    }


    printf("\n============================ RESULTS ============================\n");
    printf("%-8s %-20s %-7s %-7s %-7s %-7s %-6s\n", 
           "Roll", "Name", "S1", "S2", "S3", "Total", "Perc%");
    printf("-----------------------------------------------------------------\n");

    for (i = 0; i < n; i++) {
        printf("%-8d %-20s %-7.1f %-7.1f %-7.1f %-7.1f %-6.1f%%\n",
               s[i].roll,
               s[i].name,
               s[i].marks[0],
               s[i].marks[1],
               s[i].marks[2],
               s[i].tot,
               s[i].per);
    }
    printf("=================================================================\n");

    return 0;
}

