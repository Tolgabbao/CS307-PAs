#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main()
{   
    int pipefd[2];
    pipe(pipefd);
    int result;
    int pid = fork();
    if (pid == 0){
        // child
        // steps to set up pipe reading
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        // steps to set up pipe writing
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        char* args[] = {"./right", NULL};
        execvp(args[0], args);
        perror("execvp");
    } else {
        int num1 = 5;
        printf("Enter a number1: ");
        scanf("%d", &num1);
        int num2 = 14;
        printf("Enter a number2: ");
        scanf("%d", &num2);
        // steps to set up pipe writing
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        printf("%d %d\n", num1, num2);
        fflush(stdout);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        // wait for child to finish
        wait(NULL); // Store the exit status of the child process in 'result'
        scanf("%d", &result);
        fprintf(stderr,"result: %d\n", result);

    }
}
