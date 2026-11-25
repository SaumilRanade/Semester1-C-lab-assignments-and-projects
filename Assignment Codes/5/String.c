/*Code dump
printf("1. strlen() - Get the length of the string\n");
        printf("2. strcpy() - Copy the string to another string\n");
        printf("3. strcat() - Concatenate another string to the original string\n");
        printf("4. strcmp() - Compare the string with another string\n");
        printf("5. strrev() - Reverse the string\n");
        printf("6. strlwr() - Convert the string to lowercase\n");
        printf("7. strupr() - Convert the string to uppercase\n");
        printf("8. strncat() - Concatenate a specified number of characters from another string\n");
        printf("9. strncmp() - Compare a specified number of characters with another string\n");
        printf("10. strspn() - Get the length of the initial segment of the string that consists only of characters from another string\n");
        printf("11. strcspn() - Get the length of the initial segment of the string that consists only of characters not in another string\n");
        printf("12. strpbrk() - Find the first occurrence of any character from another string\n");
        printf("13. strrchr() - Find the last occurrence of a character in the string\n");
        printf("14. strstr() - Find the first occurrence of a substring in the string\n");
        printf("15. strsep() - Tokenize the string based on a delimiter\n");
        printf("16. strcoll() - Compare the string with another string based on the current locale\n");
        printf("17. strxfrm() - Transform the string based on the current locale\n");
        printf("18. strftime() - Format the string based on the current time and date\n");
        printf("19. strtod() - Convert the string to a double\n");
        printf("20. strtol() - Convert the string to a long integer\n");
        printf("21. strtoul() - Convert the string to an unsigned long integer\n");
        printf("22. strtof() - Convert the string to a float\n");
        printf("23. strtoll() - Convert the string to a long long integer\n");
        printf("24. strtoull() - Convert the string to an unsigned long long integer\n");
        printf("25. strsignal() - Get the string representation of a signal number\n");
        printf("26. strverscmp() - Compare two strings based on version numbers\n");
        printf("27. strnlen() - Get the length of the string up to a specified maximum length\n");
        printf("28. strndup() - Duplicate the string up to a specified maximum length\n");
        printf("29. strncat_s() - Concatenate a specified number of characters from another string with bounds checking\n");
        printf("30. strlcpy() - Copy the string to another string with bounds checking\n");
        printf("31. strlcat() - Concatenate another string to the original string with bounds checking\n");
        printf("32. strncasecmp() - Compare a specified number of characters with another string, ignoring case\n");*/
#include <stdio.h>
#include <string.h>

void clear_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void read_str(char *str, size_t max_size) {
    while (1) {
        printf("Enter a string: ");
        fgets(str, max_size, stdin);
        str[strcspn(str, "\n")] = 0; 
        
        if (strlen(str) == 0) {
            printf("Invalid string! Please enter a non-empty string.\n");
        } else {
            break;
        }
    }
}

int main() {
    char str[100]; 
    int choice = 0;

    read_str(str, sizeof(str));

    while (choice != 5) {
        printf("\nChoose an inbuilt function:\n");
        printf("1. strlen() - Get the length of the string\n");
        printf("2. strcpy() - Copy the string to another string\n");
        printf("3. strcat() - Concatenate another string to the original string\n");
        printf("4. strcmp() - Compare the string with another string\n");
        printf("5. Exit\n");

        printf("Enter your choice (1-5): ");
        while (scanf("%d", &choice) != 1 || choice < 1 || choice > 5) {
            printf("Invalid input! Please enter a number between 1 and 5: ");
            clear_buffer();
        }
        

        clear_buffer();

        switch (choice) {
            case 1:
                printf("You chose strlen()\n");
                printf("The length of the string is: %zu\n", strlen(str));
                break;
            case 2: {
                printf("You chose strcpy()\n");
                char dest[100];
                
                strcpy(dest, str);
                printf("The original string has been copied to dest: %s\n with the memory address: %p\n", dest, (void*)dest);
                break;
            }
            case 3: {
                printf("You chose strcat()\n");
                char to_add[100];
                printf("Enter string to concatenate: ");
                fgets(to_add, sizeof(to_add), stdin);
                to_add[strcspn(to_add, "\n")] = 0;
                
                strcat(str, to_add);
                printf("The concatenated string is: %s\n", str);
                break;
            }
            case 4: {
                printf("You chose strcmp()\n");
                char comp_str[100];
                printf("Enter string to compare with: ");
                fgets(comp_str, sizeof(comp_str), stdin);
                comp_str[strcspn(comp_str, "\n")] = 0;
                
                int result = strcmp(str, comp_str);
                if (result == 0) {
                    printf("The strings are identical.\n");
                } else if (result < 0) {
                    printf("The original string comes before the input string alphabetically.\n");
                } else {
                    printf("The original string comes after the input string alphabetically.\n");
                }
                break;
            }
            case 5:
                printf("Exiting the program.\n");
                break;
        }
    }
    return 0;
}