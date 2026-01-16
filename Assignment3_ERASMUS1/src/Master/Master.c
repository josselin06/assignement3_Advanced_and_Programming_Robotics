#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/file.h>

//function to write in logfile
void logFile(const char *text) {
    //open the logfile to add a new line
    FILE *logfile = fopen("log/logfile2.txt", "a"); 
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
    const char name[]="MASTER";
    //print in the file
    fprintf(logfile, "[%s] [%s] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}

int main() {
//Declare the variables
    int Mode=0;
    pid_t pid;
//Reset the logfile for a new program
    FILE *logfile = fopen("log/logfile.txt", "w"); //Replace the past values
    if (logfile == NULL) {
        perror("logfile.txt open file");
        exit(1);
    }
    fclose(logfile);
    logFile("STARTING A NEW SIMULATION");
    char text_pid[256];
    snprintf(text_pid, sizeof(text_pid),"[INFO] Program is starting with pid %d",getpid());
    logFile(text_pid);

    FILE *logfile2 = fopen("log/logfile2.txt", "w"); //Replace the past values
    if (logfile2 == NULL) {
        perror("logfile2.txt open file");
        exit(1);
    }
    fclose(logfile2);
    logFile("STARTING A NEW SIMULATION");
    //char text_pid[256];
    snprintf(text_pid, sizeof(text_pid),"[INFO] Program is starting with pid %d",getpid());
    logFile(text_pid);
//Open a file to store the PID
    logFile("[INFO] Open a file to store the PID");
    FILE *fp = fopen("log/PID_file.txt", "w"); //Replace the past values
    if (fp == NULL) {
        logFile("[ERROR] Don't open PID_file.txt");
        perror("PID_file.txt open file");
        exit(1);
    }
    fprintf(fp, "[MASTER] start with pid %d\n",getpid());
    if (fclose(fp) != 0) {
        logFile("[ERROR] Don't close PID_file.txt");
        perror("PID_file.txt close file");
        exit(1);
    }
//Read the network parameter file
    char line[256];
    FILE *f = fopen("config/network_setup.txt", "r");
    if (!f) {
        logFile("[ERROR] Don't open network_setup.txt");
        perror("Cannot open network file");
        exit(1);
    }
    // Ignore first two lines
    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);
    // Read the mode
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %d", &Mode);
    }

    fclose(f);
//Launch different Init, depending on the mode
    switch(Mode) {
    //Solo mode
        case 1 : {
            pid = fork();
            if (pid < 0) { perror("fork Init Solo"); exit(EXIT_FAILURE); }
            if (pid == 0) {
                execlp("./bin/Solo/Init","Init", NULL);
                perror("exec Init Solo");
                exit(EXIT_FAILURE);
            }
            else
            {   
                logFile("[INFO] Program Init Solo launched");
            }
            break;
        }
    //MultiDrone mode
        case 2 : {
            pid = fork();
            if (pid < 0) { perror("fork Init MultiDrone"); exit(EXIT_FAILURE); }
            if (pid == 0) {
                execlp("./bin/MultiDrone/Init","Init", NULL);
                perror("exec Init MultiDrone");
                exit(EXIT_FAILURE);
            }
            else
            {   
                logFile("[INFO] Program Init MultiDrone launched");
            }
            break;
        }
    //MultiObst mode
        case 3 : {
            pid = fork();
            if (pid < 0) { perror("fork Init MultiObst"); exit(EXIT_FAILURE); }
            if (pid == 0) {
                execlp("./bin/MultiObst/Init","Init", NULL);
                perror("exec Init MultiObst");
                exit(EXIT_FAILURE);
            }
            else
            {   
                logFile("[INFO] Program Init MultiObst launched");
            }
            break;
        }
    //Other case
        default : {
            logFile("[ERROR] Incorrect mode number");
            printf("ERROR the 'Mode' number is supposed to be 1, 2 or 3\n");
            break;
        }
    }

//Waiting for the programs to finish
    while (wait(NULL) > 0);
//Finish cleanly
    logFile("[INFO] Finish cleanly");
    return 0;
}
