/* CS511-HW1
 * lin liu 10397798
 * 09.27.2015
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define tmpfifo "./primesTestTmpFIFO"

struct process_info {
    pid_t pid;
    int fd;
    int count;
};
struct process_info *child;
int num_children;
int biggestfd;

/* Calculate the prime number */
void prime(int begin, int end, int writefd, int process) 
{   
    int i, num;
    /* double c = sqrt(end); */
    int n = end - begin + 1;
    int *bv = calloc(n, sizeof(int));
    for (i = 2; i*i <= end; i++) {
        int j;
        for (j = begin / i; j <= (end/i); j++) {   
            int s = i*j-begin;
            if(s >= 0 && s <= (n-1) && j != 1)
                bv[s]=1;
        }
    }
    num = 0;
    for (i = 0; i < n; i++) {
        if (bv[i] == 0) {
            int tmp = i + begin;
            write(writefd, &tmp, sizeof(int));
            num++;
        }
    }
    close(writefd);
    exit(num);
}

/* Create fifo and child process */
void doFIFO(int begin, int end, int process)
{
    pid_t pid;
    int writefd, readfd;
    char str[50] = tmpfifo;
    char strnum[15];
    sprintf(strnum, "%d", process);
    strcat(str, strnum);

    if (mkfifo(str, 0600)) {
        fprintf(stderr, "Unable to create %s : %s\n", str,
				strerror(errno));
		exit(EXIT_FAILURE);
    }

    if ((pid = fork()) < 0) {
        fprintf(stderr, "fork fail : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        /* Parent process */
        printf("child %i: bottom=%i, top=%i\n", pid, begin, end);
        if ((readfd = open(str, O_RDONLY)) == -1) {
            fprintf(stderr, "Unable to open %s : %s\n", str,
	    			strerror(errno));
	    	exit(EXIT_FAILURE);
        }
        child[process].pid = pid;
        child[process].fd = readfd;
        child[process].count = 0;
        if (biggestfd < readfd) biggestfd = readfd;
    } else {
        /* Child process */
        if ((writefd = open(str, O_RDWR)) == -1) {
            fprintf(stderr, "Unable to open %s : %s\n", str,
	    			strerror(errno));
	    	exit(EXIT_FAILURE);
        }
        prime(begin, end, writefd, process);
    }
}

/* Create pipe and child process */
void doPipe(int begin, int end, int process)
{
    pid_t pid;
    int fd[2];
    if (pipe(fd) < 0) {
        fprintf(stderr, "pipo fail : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if ((pid = fork()) < 0) {
        fprintf(stderr, "fork fail : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else if (pid > 0){
        printf("child %i: bottom=%i, top=%i\n", pid, begin, end);
        close(fd[1]);
        child[process].pid = pid;
        child[process].fd = fd[0];
        child[process].count = 0;
        if (biggestfd < fd[0]) biggestfd = fd[0];
    } else {
        close(fd[0]);
        prime(begin, end, fd[1], process);
    }
}

/* Get process information from fd */
struct process_info* getinfo(int fd)
{
    int i;
    for (i = 0; i < num_children; i++)
        if (child[i].fd == fd)
            return &child[i];

    return NULL;
}

/* Set the readfds */
void set_readfds(fd_set* readfds)
{
    int i;
    FD_ZERO(readfds);
    for (i = 0; i < num_children; i++) {
        if (child[i].fd != -1)
            FD_SET(child[i].fd, readfds);
    }
}

void get_select() 
{
    int rc, prime, status, i, ready;
    fd_set readfds;
    struct process_info* proc;
    int live_child = num_children;

    while (live_child > 0) {
        set_readfds(&readfds);
        if ((ready = select(biggestfd + 1, &readfds, NULL, NULL, NULL)) < 0) {
            fprintf(stderr, "Select error : %s\n", strerror(errno));
		    exit(EXIT_FAILURE);    
        } else if (ready > 0) {
            for (i = 0; i < (biggestfd + 1); i++) {
                if (FD_ISSET(i, &readfds)) {
                    rc = read(i, &prime, sizeof(int));
                    proc = getinfo(i);
                    if (rc > 0) {
                        printf("%i is prime\n", prime);
                        (proc->count)++;
                    }
                    else if (rc < 0) {
                        fprintf(stderr, "Source is unable to read : %s\n",
		            			strerror(errno));
		            	exit(EXIT_FAILURE);    
                    } else {
                        /* One child process finish its work */
                        close(proc->fd);
                        FD_CLR(proc->fd, &readfds);
                        proc->fd = -1;
                        live_child--;
                        waitpid(proc->pid, &status, 0);
                        if (WEXITSTATUS(status) == proc->count)
                            printf("child %i exited correctly\n", proc->pid);
                    }
                }
            }
        }
    }
}

/* Remove all the temporary FIFO */
void removeFIFO()
{   
    int i;
    for (i = 0; i < num_children; i++) {
        if (i%2) {
            char str[50] = tmpfifo;
            char strnum[15];
            sprintf(strnum, "%d", i);
            strcat(str, strnum);
            remove(str);
        }
    }
}

int
main(int argc, char *argv[])
{
    int bottom, top, i;
    biggestfd = 0;
    num_children = argc - 1;
    child = calloc(num_children, sizeof(struct process_info));
    bottom = 2;
    
    if (argc < 2) {
        fprintf(stderr, "usage: %s <increasing positive integers>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    for (i = 1; i < argc; i++) {
        top = atoi(argv[i]);
        if (i%2) doPipe(bottom ,top, i - 1);
        else doFIFO(bottom, top, i - 1);
        bottom = top + 1;
    }

    get_select();
    removeFIFO();
    exit(EXIT_SUCCESS);
}