#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <sys/file.h>

//function to write in logfile
void logFile(const char *text) {
    //open the logfile to add a new line
    FILE *logfile = fopen("log/logfile.txt", "a"); 
    if (!logfile) {
        perror("logfile open");
        exit(1);
    }
    int fd = fileno(logfile);
    if (flock(fd, LOCK_EX) != 0) {//lock the file to be available to write
        perror("flock");
        fclose(logfile);
        exit(1);
    }
    //create the time label
    struct timeval tv;
    gettimeofday(&tv, NULL); //read seconds and microseconds
    struct tm* tm_info = localtime(&tv.tv_sec);//change seconds into hour/minut/second
    static char time[32];
    snprintf(time, sizeof(time),"%02d:%02d:%02d.%03d",tm_info->tm_hour,tm_info->tm_min,tm_info->tm_sec,(int)(tv.tv_usec/1000));
    //create the name label
    const char name='T';
    //print in the file
    fprintf(logfile, "[%s] [%c] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}
//function to generate random number in a certain range
int random_int(int min, int max) {
    // read the random number form the faile /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    int rand_val;
    if (read(fd, &rand_val, sizeof(rand_val)) != sizeof(rand_val)) {
        perror("read");
        close(fd);
        return -1;
    }
    close(fd);
    //only positive numbers
    int pos_val;
    if(rand_val<0)
        pos_val=-rand_val;
    else
        pos_val=rand_val;
    // re-size in the intervale [min, max]
    return min + (pos_val % (max - min + 1));
}
//Signal handler stop
volatile sig_atomic_t running = 1;
void handler_stop(int s) {
    running = 0;
}
//Signal handler answer watchdog
volatile char messagetowatchdog[256] = "Initialization\n";
void * watchdog_thread() {
//wait for the master to launch the watchdog
    usleep(10000);//0.01s
//get watchdog pid from pid file
    FILE *fp = fopen("log/PID_file.txt", "r");
    if (fp == NULL) {
        perror("PID_file.txt not open");
        exit(1);
    }
    pid_t watchdog_pid;
    char line[256];
    //read the pid of each line, the last one read is the one of the watchdog
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%*[^0-9]%d", &watchdog_pid);//update the value of the pid until the end
    }
    fclose(fp);
//open fifo to watchdog
    //Creating pipe to send messages to the watchdog
    char * fifoTtoW = "/tmp/fifoTtoW";
    int fdW = open(fifoTtoW, O_WRONLY);
    if (fdW < 0) {
        perror("open fifo");
        exit(1);
    }
//main loop
    while(running){
        write(fdW, (const void *)messagetowatchdog, sizeof(messagetowatchdog));
        kill(watchdog_pid,SIGRTMIN+4);
        usleep(200000);//0.2s
    }
//Terminate programme
    close(fdW);
    return 0;
}

int main() {
//Starting
    char text_pid[256];
    snprintf(text_pid, sizeof(text_pid),"[INFO] Program is starting with pid %d",getpid());
    logFile(text_pid);
//Declare the variables
    //for the targets
    int Targets[2][9];
//Open the pipe
    //Creating pipe from Blackboard  to Targets
    char * fifoBtoT = "/tmp/fifoBtoT"; 
    //Creating pipe from Targets to Blackboard  
    char * fifoTtoB = "/tmp/fifoTtoB"; 

    int fd1 = open(fifoTtoB, O_WRONLY);
    int fd2 = open(fifoBtoT, O_RDONLY);

    if (fd1 < 0 || fd2 < 0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }
//Signal setup
    //handler_stop
    struct sigaction sa;
    sa.sa_handler = handler_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGTERM, &sa, NULL);
//Thread setup
    pthread_t watchdogThread;
    pthread_create(&watchdogThread, NULL, watchdog_thread, NULL);
    logFile("[INFO] Start talking with Watchdog");
//Main loop
    logFile("[INFO] Start creating targets every minutes");
    while (running) {
    //Generate new values
        strcpy((char *)messagetowatchdog, "Generating new values\n");
        for (int j = 0; j < 9; j++) {
            Targets[0][j]=random_int(1,99);
        }
        for (int j = 0; j < 9; j++) {
            Targets[1][j]=random_int(1,39);
        }
    //send the targets to blackboard
        strcpy((char *)messagetowatchdog, "Send the targets to blackboard\n");
        //ask to write
        write(fd1, "w", 1);

        //send the values of the tab when the blackboard is ready
        strcpy((char *)messagetowatchdog, "Send the values of the tab when the blackboard is ready\n");
        char buffer[128];
        int n = read(fd2, buffer, sizeof(buffer));
        if (n > 0 && strncmp(buffer, "ok", 2) == 0) {
            char buffer[1024];
            int pos = 0;

            for (int i = 0; i < 2; i++)
            {
                for (int j = 0; j < 9; j++) {
                    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%d ", Targets[i][j]);
                }
            }
            buffer[pos++] = '\n';

            write(fd1, buffer, pos);
        }
    //
        strcpy((char *)messagetowatchdog, "Sleep\n");
        sleep(60); //wait 1 minute because it is impossible to reach all the target in less than 1 minute
    }
//Terminate program
    strcpy((char *)messagetowatchdog, "Terminate program\n");
    logFile("[INFO] Finish cleanly");
    close(fd1);
    close(fd2);
    pthread_detach(watchdogThread);
    return 0;
}
