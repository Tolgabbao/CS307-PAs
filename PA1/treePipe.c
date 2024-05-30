#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // Check for correct number of arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <current depth> <max depth> <left-right>\n", argv[0]);
        return 1;
    }

    // Convert arguments to integers
    int curDepth = atoi(argv[1]);
    int maxDepth = atoi(argv[2]);
    int lr = atoi(argv[3]);
    int result;

    // Get num1 input
    int num1;
    int num2;
    if (curDepth == 0) {
        // Root node: get input from user
        // Print current depth and lr value
        num2 = 0;
        for (int i = 0; i < curDepth * 3; i++) {
            fprintf(stderr, "-");
        }
        fprintf(stderr, "> current depth: %d, lr: %d\n", curDepth, lr);
        fprintf(stderr, "Please enter num1 for the root: ");
        scanf("%d", &num1);
        fprintf(stderr, "> my num1 is: %d\n", num1);
    } else {
        // Non-root node: read num1 from parent through pipe
        scanf("%d", &num1);
        scanf("%d", &num2);
        // printout num1
        for (int i = 0; i < curDepth * 3; i++) {
            fprintf(stderr, "-");
        }
        fprintf(stderr, "> current depth: %d, lr: %d, my num1: %d, my num2: %d\n", curDepth, lr, num1, num2);
    }

    if (curDepth == maxDepth && maxDepth == 0){
        num2 = 1;
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
            char* args[] = {lr == 0 ? "./left" : "./right", NULL};
            execvp(args[0], args);
            perror("execvp");
        } else {
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
            fprintf(stderr,"> my result is: %d\n", result);
            fprintf(stderr,"The final result is: %d\n", result);
        }
    }


    // num1 gets passed down to left nodes and num2 gets returned back up
    // visit left nodes recursively
    if (curDepth == maxDepth) {
        num2 = 1;
        // Calculate the result
        int pipe_work[2];
        pipe(pipe_work);
        int pid_work = fork();
        if (pid_work == 0) {
            // Child process
                    // steps to set up pipe reading
            dup2(pipe_work[0], STDIN_FILENO);
            close(pipe_work[0]);
            // steps to set up pipe writing
            char* args[] = {lr == 0 ? "./left" : "./right", NULL};
            execvp(args[0], args);
            perror("execvp");
        } else {
            // Parent process
            // Wait for child to finish
            dup2(pipe_work[1], STDOUT_FILENO);
            close(pipe_work[1]);
            if (curDepth == maxDepth) {
                num2 = 1;
                printf("%d %d\n", num1, num2);
            }
            else {
                printf("%d %d\n", num1, num2);
            }
            fflush(stdout);
            wait(NULL);
        }
    }

    if (curDepth < maxDepth) {
        // Create a child process
        // Create a pipe
        int pipefd[2];
        pipe(pipefd);
        int pid = fork();
        if (pid == 0) {
            // Child process
            // Set up pipe reading
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            // Set up pipe writing
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            // Call execvp on left file
            // debug fprintf(stderr, "calling left treePipe\n");
            // current depth is incremented by 1
            curDepth++;
            // init args using curDepth and maxDepth without using argv[1] and argv[2] int to string conversion
            char curDepthStr[10];
            char maxDepthStr[10];
            sprintf(curDepthStr, "%d", curDepth);
            sprintf(maxDepthStr, "%d", maxDepth);
            char* args[] = {"./treePipe", curDepthStr, maxDepthStr, "0", NULL};
            execvp(args[0], args);
            perror("execvp");
        } else {
            // Parent process
            // Set up pipe writing
            // Print num1 to pipe
            dup2(pipefd[1], STDOUT_FILENO);
            dprintf(pipefd[1], "%d\n", num1);
            dprintf(pipefd[1], "%d\n", num2);
            // Set up pipe reading
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            // Wait for child to finish
            wait(NULL);
            // debug fprintf(stderr, "reading num2 from left pipe, curDepth: %d\n", curDepth);
            // Read num2 from pipe
            scanf("%d", &num2);
            // debug fprintf(stderr, "num2 from left pipe: %d\n", num2);
            // Calculate the result
            int pid_work = fork();
            if (pid_work == 0) {
                // Child process
                char* args[] = {lr == 0 ? "./left" : "./right", NULL};
                execvp(args[0], args);
                perror("execvp");
            } else {
                // Parent process
                // Wait for child to finish
                if (curDepth == maxDepth) {
                    num2 = 1;
                    printf("%d %d\n", num1, num2);
                }
                else {
                    printf("%d %d\n", num1, num2);
                }
                fflush(stdout);
                wait(NULL);
                scanf("%d", &result);
                // write result to pipe for parent to read
                for (int i = 0; i < (curDepth+1) * 3; i++) {
                    fprintf(stderr, "-");
                }
                fprintf(stderr, "> my result is: %d\n", result);
                printf("%d\n", result);
            }
        }
    }


    // visit right nodes recursively
    if (curDepth < maxDepth) {
        // Create a child process
        int fdright[2];
        pipe(fdright);
        int pid = fork();
        if (pid == 0) {
            // Child process
            // Set up pipe reading
            dup2(fdright[0], STDIN_FILENO);
            close(fdright[0]);
            // Set up pipe writing
            dup2(fdright[1], STDOUT_FILENO);
            close(fdright[1]);
            // Call execvp on right file
            // debug fprintf(stderr, "calling right treePipe\n");
            // current depth is incremented by 1
            curDepth++;
            char curDepthStr[10];
            char maxDepthStr[10];
            sprintf(curDepthStr, "%d", curDepth);
            sprintf(maxDepthStr, "%d", maxDepth);
            char* args[] = {"./treePipe", curDepthStr, maxDepthStr, "1", NULL};
            execvp(args[0], args);
            perror("execvp");
        } else {
            // Parent process
            // Set up pipe writing
            dup2(fdright[1], STDOUT_FILENO);
            close(fdright[1]);
            // Print num1 to pipe
            printf("%d\n", num1);
            printf("%d\n", num2);
            fflush(stdout);
            // Set up pipe reading
            dup2(fdright[0], STDIN_FILENO);
            close(fdright[0]);
            // Wait for child to finish
            wait(NULL);
            // debug fprintf(stderr, "reading num2 from right pipe, curDepth: %d\n", curDepth);
            // Read num2 from pipe
            scanf("%d", &num2);
            // debug fprintf(stderr, "num2 from right pipe: %d\n", num2);
            printf("%d\n", num2);
        }
    }
    if (curDepth == 0 && maxDepth != 0){
        fprintf(stderr, "The final result is: %d\n", num2);
    }
    return 0;
}
