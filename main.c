#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>


//Each set of 2 addresses is a single pipe, so use PIPES as representation of that
#define PIPES 2


//3 Pipes used (read = 0, write = 1)
// - Write to next child [cNum]
// - read from previous child [cNum - 1]
// - Write to parent

void closePipeRead(int cTot, int *pipe) {
    for (int i = 0; i < cTot; ++i) {
        close(pipe[2 * i]);
    }
}

void closePipeWrite(int cTot, int *pipe) {
    for (int i = 0; i < cTot; ++i) {
        close(pipe[2 * i + 1]);
    }
}

void closePipe(int cNum, int cTot, int *pipe) {
    for (int i = 0; i < cTot; ++i) {
        int numWrap = (cNum + cTot - 1) % cTot;
        if (i == numWrap) {
            close(pipe[2 * i + 1]);
        }
        else if( i == cNum) {
            close(pipe[2 * i]);
        }
        else {
            close(pipe[2*i]);
            close(pipe[2*i+1]);;;
        }
    }
}


int main(void) {
    int *fd_c;
    int fd_p_all[2];
    int fd_p_c0[2];

    int read_c_prev;
    int write_c_next;

    int childCount;
    char buff[4];
    int childNum = 0;
    int evens = 0;
    int odds = 0;
    int runCount = 1;
    int restart = 0;


    int pid;
    size_t errorCheck;
    size_t check;
    int num = 0;
    int nextNum = 0;


    printf("Please enter the number (<1000) of children for the Collatz circle: ");
    fgets(buff, 4, stdin);
    childCount = strtol(buff, NULL, 10);


    //A pipe is an array of 2 ints, the first element being read, the second being write.
    //As such, if we want to make them dynamically, we need to create an int pointer that
    //points towards an appropriately sized amount of memory. We need 2 pipes per child,
    //1 pipe to read/write to the next child, and one pipe to read/write to the parent.
    //fd_c will be the pipes for child to child communication

    fd_c = malloc(PIPES * (childCount) * sizeof *fd_c);

    for (int i = 0; i < childCount; ++i) {
        if (pipe(&fd_c[i * 2]) < 0) {
            perror("Pipe initialization failed\n");
            exit(1);
        }
    }

    if (pipe(fd_p_c0) < 0) {
        perror("Pipe initialization failed\n");
        exit(1);
    }
    if (pipe(fd_p_all) < 0) {
        perror("Pipe initialization failed\n");
        exit(1);
    }

    //Fork childCount times, break if child so not exponential
    for (int i = 0; i < childCount; ++i) {
        if ((errorCheck = fork()) < 0) {
            perror("fork failed\n");
            exit(1);
        }
        if (errorCheck == 0) {
            childNum = i;
            break;
        }
        sleep(0.2);
    }
    //Set PID of each process
    pid = getpid();

    //CHILD PROCESS OPERATIONS
    if (errorCheck == 0) {
        //Set pipes for child communication. Read from prev, write to next, write to par
        int numWrap = (childNum + childCount - 1) % childCount;
        read_c_prev = fd_c[numWrap * 2];
        write_c_next = fd_c[childNum * 2 + 1];


        //Close Unused pipes
        closePipe(childNum, childCount, fd_c); //Close all the other pipes in fd_c

        close(fd_p_all[0]);
        close(fd_p_c0[1]);

        //First run through, pass ready to parent before starting loop
        int ready = 1;
        check = write(fd_p_all[1], &ready, sizeof ready);
        if (check < 0) perror("Error Writing: ");


        //usleep(100);

        //////////////////////
        //Child process loop//
        //////////////////////
        while (1) {
            printf("Child %d is ready.\n", pid);
            sleep(0.5);
            //first child receives number from parent on first cycle
            if (childNum == 0 && runCount == 1) {
                int xcheck = read(fd_p_c0[0], &num, sizeof num);
                if (xcheck < sizeof num) {
                    printf("Error in C0 read from parent!\n");
                    if (xcheck < 0) perror("Error: ");
                }else {
                    runCount++;
                    printf("Child %d received %d from parent!\n", pid, num);
                }
            }
            //All other runs besides the first
            else {
                check = read(read_c_prev, &num, sizeof num);
                if (check < sizeof num) {
                    printf("Error in read from previous child!\n");
                    perror("Error: ");
                }
                if(num == -1) {
                    printf("Child %d has finished Current Collatz. Resetting for next input.\n", pid);
                    if(restart == 0) {
                        nextNum = -1;
                        write(write_c_next, &nextNum, sizeof nextNum);
                    }
                    else {
                        restart = 0;
                    }
                    runCount = 1;
                    continue;
                }
                else {
                    printf("Received %d from previous child\n", num);
                }
            }



            if (num == 1) {
                restart = -1;
                nextNum = -1;
                write(fd_p_all[1], &restart, sizeof restart);
                write(write_c_next, &nextNum, sizeof nextNum);
                odds++;
                continue;
            }
            //if  end of sequence, write even count and odd count to parent
            //then write to next child to continue sequence.
            if (num == 0) {
                printf("Child %d received exit command.\n", pid);
                sleep(0.5);
                printf("Child %d is done. Exiting now.\n", pid);
                nextNum = 0;
                int errWrite;
                errWrite = write(fd_p_all[1], &pid, sizeof pid);
                if (errWrite < 0) perror("Error writing PID");
                errWrite = write(fd_p_all[1], &evens, sizeof evens);
                if (errWrite < 0) perror("Error writing evens");
                errWrite = write(fd_p_all[1], &odds, sizeof odds);
                if (errWrite < 0) perror("Error writing odds");

                if(childNum < childCount -1)
                    write(write_c_next, &nextNum, sizeof nextNum);
                break;
            }

            //Determine if odd or even
            if (num % 2 == 0) {
                evens++;
                nextNum = num / 2;
            } else {
                odds++;
                nextNum = 3 * num + 1;
            }
            sleep(1);
            write(write_c_next, &nextNum, sizeof nextNum);
            sleep(1);
        }
        //Exiting child process
        close(read_c_prev);
        close(write_c_next);
        close(fd_p_c0[0]);

        free(fd_c);
        exit(0);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////

    //PARENT PROCESS OPERATIONS
    else {
        //Close all unused pipes
        closePipeWrite(childCount, fd_c); //Don't need fd_c at all in parent
        closePipeRead(childCount, fd_c);
        close(fd_p_c0[0]);
        close(fd_p_all[1]);

        //Wait for ready from all children
        int readyStatus[childCount];
        int readyCount = 0;
        sleep(2);
        for (int i = 0; i < childCount; i++) {
            int zcheck = read(fd_p_all[0], &readyStatus[i], sizeof readyStatus[i]);
            if (zcheck < 0) perror("Error reading");
            if (readyStatus[i] == 1) {
                readyCount++;
            }
        }
        if (readyCount != childCount) {
            printf("Children are not ready to start.\n");
            //exit(1);
        }
        while (1) {
            //Get number from input
            printf("Please enter the starting number(<1000) for the Collatz circle: ");
            fgets(buff, 4, stdin);
            num = strtol(buff, NULL, 10);

            //Write to C0 initial number
            write(fd_p_c0[1], &num, sizeof num);


            if (num == 0) break;

            int done;
            int readCheck = read(fd_p_all[0], &done, sizeof done);
            if (readCheck < 0) perror("Error reading");
            sleep(2);
            printf("Collatz Loop Complete.\n");
        }

        while (wait(NULL) > 0);
        //Could do in a single loop, but if doing it in two loops, can wait and print them all at once.
        int status[childCount * 3];
        for (int i = 0; i < childCount * 3; i++) {
            int readCheck = read(fd_p_all[0], &status[i], sizeof status[i]);
            if (readCheck < 0) perror("Error reading");
        }
        for (int i = 0; i < childCount; i++) {
            printf("Child %d had %d even numbers and %d odd numbers.\n",
                   status[3 * i], status[3 * i + 1], status[3 * i + 2]);
        }

        printf("Children have exited. Parent exiting.\n");

        close(fd_p_c0[1]);
        close(fd_p_all[0]);
        free(fd_c);
    }

    return 0;
}
