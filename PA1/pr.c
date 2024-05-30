#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int num1, num2;
     if (argc != 1) {
        printf("Usage: %s \n", argv[0]);
        return 1; // Error code for incorrect usage
    }
    scanf("%d", &num1);
    scanf("%d", &num2);
    // Calculate the multiplication
    int result = num2 * num1;
    // Print the result
    printf("%d\n", result);
    //fprintf(stderr, "Subprocess result: %d\n", result);
    //fprintf(stderr, "my num1 is: %d, my num2 is: %d", num1, num2);

    return 0; // Successful execution
}