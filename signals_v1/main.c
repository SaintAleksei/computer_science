#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#define ERR(condition)\
    do\
    {\
        if (condition)\
        {\
            fprintf (stderr, "ERROR: %s:%d in function %s: %s\n",\
                             __FILE__, __LINE__, __PRETTY_FUNCTION__, strerror (errno) );\
            exit (EXIT_FAILURE);\
        }\
    }\
    while (0)

#define ALRM_TIME 3

void usr1_handler (int signum);
void usr2_handler (int signum);
void chld_handler (int signum);
void alrm_handler (int signum);

uint8_t end;

struct buffer
{
    uint8_t data[BUFSIZ];
    size_t  byte;
    uint8_t bit;
} buff;

struct sigaction act = {};

pid_t pid;

int main (int argc, char **argv)
{    
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s <name_of_file>\n", argv[0]);
        
        exit (EXIT_FAILURE);
    }

    sigset_t set = {};

    sigemptyset (&set);
    sigaddset (&set, SIGUSR1);
    sigaddset (&set, SIGUSR2);
    sigaddset (&set, SIGALRM);
    ERR (sigprocmask (SIG_SETMASK, &set, NULL) == -1);

    sigemptyset (&set);

    act.sa_handler = alrm_handler;
    ERR (sigaction (SIGALRM, &act, NULL) == -1);

    sigaddset (&act.sa_mask, SIGALRM);

    struct sigaction act = {.sa_handler = usr1_handler};
    ERR (sigaction (SIGUSR1, &act, NULL) == -1);

    act.sa_handler = usr2_handler;
    ERR (sigaction (SIGUSR2, &act, NULL) == -1);

    sigdelset (&act.sa_mask, SIGALRM);    

    act.sa_handler = chld_handler;
    ERR (sigaction (SIGCHLD, &act, NULL) == -1);

    pid_t pid = fork ();
    ERR (pid == -1);

    if (pid > 0) /* sender */
    {
        int fd = open (argv[1], O_RDONLY);
        ERR (fd == -1);

        while (1)
        {
            ssize_t nbytes = read (fd, buff.data, BUFSIZ);
            ERR (nbytes < 0);

            if (!nbytes)
                exit (EXIT_SUCCESS);
        
            buff.byte = 0;
            buff.bit  = 0;

            while (buff.byte < (size_t) nbytes)
            {
                if (buff.data[buff.byte] & (1 << buff.bit) )
                    kill (pid, SIGUSR1);
                else
                    kill (pid, SIGUSR2); 

                sigsuspend (&set);

                if (buff.bit >= 8)  
                {
                    buff.bit   = 0;
                    buff.byte += 1;
                }
            } 
        }
    }
    else /* receiver */
    {
        pid = getppid ();
        ssize_t nbytes = 0;

        while (1)
        {
            alarm (ALRM_TIME);        

            sigsuspend (&set);

            alarm (ALRM_TIME);

            if (end)
            {
                nbytes = write (STDOUT_FILENO, buff.data, buff.byte);
                ERR (nbytes == -1);

                exit (EXIT_SUCCESS);
            }

            if (buff.bit >= 8)
            {
                buff.byte++;
                buff.bit = 0;
            }

            if (buff.byte == BUFSIZ)
            {
                nbytes = write (STDOUT_FILENO, buff.data, BUFSIZ);
                ERR (nbytes == -1);

                buff.byte = 0;
            }

            kill (pid, SIGUSR1);
        }
    }
}

void usr1_handler (int signum)
{
    buff.data[buff.byte] |= (1 << buff.bit);
    buff.bit++;
}

void usr2_handler (int signum)
{
    buff.data[buff.byte] &= ~(1 << buff.bit);
    buff.bit++;
}

void chld_handler (int signum)
{
    _exit (EXIT_SUCCESS);
}

void alrm_handler (int signum)
{
    end = 1;
}
