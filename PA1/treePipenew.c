#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>


/*1:
Receive num1 from command line
Create left branch 2
Pass num1 to left branch 2 i1
Wait for process 2 to finish
Scan num2 from process 2 i6
Calculate result with execvp calc4
Create right branch 5
Pass result to right branch 5 i7
Wait for process 5 to finish



2:
Receive num1 from i1
Create left branch 3
Pass num1 to left branch 3 i2
Wait for process 3 to finish
Scan num2 from process 3 i3
Calculate result with execvp calc2
Create right branch 4
Pass result to right branch 4 i4
Wait for process 4 to finish
Scan ret from process 4
Pass ret to parent i6
EXIT
3:
Scan num1 from i2
Set num2 = 1 due to leaf node
Calculate result with execvp calc1
Pass result back to parent i3 -> 2
EXIT

4:
Scan num1 from i4
Set num2  = 1 due to leaf node
Calculate result with execvp calc3
Pass result back to parent i5 -> 2
EXIT

5:
Scan num1 from i7 
Create left branch 6
Pass num1 to left branch 6 i8
Wait for process 6 to finish
Scan num2 from process 6 i9
Calculate result with execvp calc6
Create right branch 7
Pass result to right branch 7 i10
Wait for process 7 to finish
Scan ret from process 7 i11
Pass ret to parent i12
*/

int main(int argc, char *argv[]){
    // Check for correct number of arguments
    if (argc != 4){
        fprintf(stderr, "Usage: %s <current depth> <max depth> <left-right>\n", argv[0]);
        return 1;
    }

    // Convert arguments to integers
    int curDepth = atoi(argv[1]);
    int maxDepth = atoi(argv[2]);
    int lr = atoi(argv[3]);
    for (int i = 0; i < curDepth * 3; i++){
        fprintf(stderr, "-");
    }
    fprintf(stderr, "> current depth: %d, lr: %d\n", curDepth, lr);

    int num1;
    if (curDepth == 0){
        // Root node: get input from user
        fprintf(stderr, "Please enter num1 for the root: ");
        scanf("%d", &num1);
    } else {
        // Non-root node: read num1 from parent through pipe
        scanf("%d", &num1);
    }
    for (int i = 0; i < curDepth * 3; i++){
        fprintf(stderr, "-");
    }
    fprintf(stderr, "> my num1 is: %d\n", num1);
    

    // Create left branch
    int num2;
    if (curDepth != maxDepth){
        int pipefd[2];
        pipe(pipefd);
        int pid = fork();
        if (pid == 0){
            // child
            // steps to set up pipe reading
            dup2(pipefd[0], STDIN_FILENO);
            // steps to set up pipe writing
            dup2(pipefd[1], STDOUT_FILENO);
            char curDepthStr[11];
            char maxDepthStr[11];
            sprintf(curDepthStr, "%d", curDepth + 1);
            sprintf(maxDepthStr, "%d", maxDepth);
            char *args[] = {"./treePipe", curDepthStr, maxDepthStr, "0", NULL};
            execvp(args[0], args);
            perror("execvp");
        }
        else {
            // steps to set up pipe writing
            dup2(pipefd[1], STDOUT_FILENO);
            dprintf(pipefd[1], "%d\n", num1);
            fflush(stdout);
            dup2(pipefd[0], STDIN_FILENO);
            // wait for child to finish
            wait(NULL);
            scanf("%d", &num2);
        }
    }

    // Calculate result
    int result;
    if (curDepth == maxDepth) num2 = 1;
    // Calculate the result
    int pipe_work[2];
    pipe(pipe_work);
    int pid_work = fork();
    if (pid_work == 0) {
        // Child process
        // steps to set up pipe reading
        dup2(pipe_work[0], STDIN_FILENO);
        dup2(pipe_work[1], STDOUT_FILENO);
        // steps to set up pipe writing
        char* args[] = {lr == 0 ? "./left" : "./right", NULL};
        execvp(args[0], args);
        perror("execvp");
    } else {
        // Parent process
        // Wait for child to finish
        if (curDepth == maxDepth) {
            num2 = 1;
            dprintf(pipe_work[1], "%d %d\n", num1, num2);
        }
        else {
            dprintf(pipe_work[1], "%d %d\n", num1, num2);
        }
        fflush(stdout);
        wait(NULL);
        scanf("%d", &result);
        printf("%d\n", result);
    }


    for (int i = 0; i < curDepth * 3; i++){
        fprintf(stderr, "-");
    }
    fprintf(stderr, "> current depth: %d, lr: %d, my num1: %d, my num2: %d\n", curDepth, lr, num1, num2);
    for (int i = 0; i < curDepth * 3; i++){
        fprintf(stderr, "-");
    }
    fprintf(stderr, "> my result is: %d\n", result);
    // Create right branch
    int ret;
    if (curDepth != maxDepth){
        int pipefd[2];
        pipe(pipefd);
        int pid = fork();
        if (pid == 0){
            // child
            // steps to set up pipe reading
            dup2(pipefd[0], STDIN_FILENO);
            // steps to set up pipe writing
            dup2(pipefd[1], STDOUT_FILENO);
            char curDepthStr[11];
            char maxDepthStr[11];
            sprintf(curDepthStr, "%d", curDepth + 1);
            sprintf(maxDepthStr, "%d", maxDepth);
            char *args[] = {"./treePipe", curDepthStr, maxDepthStr, "1", NULL};
            execvp(args[0], args);
            perror("execvp");
        }
        else {
            // steps to set up pipe writing
            dprintf(pipefd[1], "%d\n", result);
            dup2(pipefd[0], STDIN_FILENO);
            // wait for child to finish
            wait(NULL);
            scanf("%d", &ret);
            fprintf(stderr, "my ret is: %d", ret);
            printf("%d\n", ret);
            fflush(stdout);
        }
    }


    // Print result to parent

}