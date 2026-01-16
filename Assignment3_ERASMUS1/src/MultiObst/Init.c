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
    const char name[]="INIT";
    //print in the file
    fprintf(logfile, "[%s] [%s] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}

int main() {
//Starting
    char text_pid[256];
    snprintf(text_pid, sizeof(text_pid),"[INFO] Program is starting with pid %d",getpid());
    logFile(text_pid); 
//Creating fifo for the blackboard communication
    //Creating pipe from Blackboard  to Drone
    char * fifoBtoD = "/tmp/fifoBtoDa"; 
    mkfifo(fifoBtoD, 0666);
    //Creating pipe from Drone to Blackboard  
    char * fifoDtoB = "/tmp/fifoDtoBa"; 
    mkfifo(fifoDtoB, 0666);
    //Creating pipe from Blackboard  to Input
    char * fifoBtoI = "/tmp/fifoBtoIa"; 
    mkfifo(fifoBtoI, 0666);
    //Creating pipe from Input to Blackboard  
    char * fifoItoB = "/tmp/fifoItoBa"; 
    mkfifo(fifoItoB, 0666);
    //Creating pipe from Input to Init
    char * fifoCtoI = "/tmp/fifoCtoIa"; 
    mkfifo(fifoCtoI, 0666);
    //Creating pipe from Blackboard  to Client
    char * fifoBtoC = "/tmp/fifoBtoCa"; 
    mkfifo(fifoBtoC, 0666);
    //Creating pipe from Client to Blackboard  
    char * fifoCtoB = "/tmp/fifoCtoBa"; 
    mkfifo(fifoCtoB, 0666);

//Open a file to store the PID
    logFile("[INFO] Open a file to store the PID");
    FILE *fp = fopen("log/PID_file.txt", "a");
    if (fp == NULL) {
        logFile("[ERROR] Don't open PID_file.txt");
        perror("PID_file.txt open file");
        exit(1);
    }
    fprintf(fp, "[INIT] start with pid %d\n",getpid());
//Fork+exec to launch the programmes
    logFile("[INFO] Start launching all the programs");

    pid_t pid;
    pid_t list_pid[4];

    // Open B in konsole
    pid = fork();
    if (pid < 0) { perror("fork B"); exit(EXIT_FAILURE); }
    if (pid == 0) {
        execlp("konsole", "konsole", "-e", "./bin/MultiObst/B", NULL);
        perror("exec B");
        exit(EXIT_FAILURE);
    }
    else
    {   
        fprintf(fp, "[B] start with pid %d\n",pid);
        logFile("[INFO] Program B launched");
        list_pid[0]=pid;
    }

    // Open D
    pid = fork();
    if (pid < 0) { perror("fork D"); exit(EXIT_FAILURE); }
    if (pid == 0) {
        //execlp("konsole", "konsole", "-e", "./bin/MultiObst/D", NULL);
        execlp("./bin/MultiObst/D", "D", NULL);
        perror("exec D");
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(fp, "[D] start with pid %d\n",pid);
        logFile("[INFO] Program D launched");
        list_pid[1]=pid;
    }

    // Open I
    pid = fork();
    if (pid < 0) { perror("fork I"); exit(EXIT_FAILURE); }
    if (pid == 0) {
        execlp("konsole", "konsole", "-e", "./bin/MultiObst/I", NULL);
        perror("exec I");
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(fp, "[I] start with pid %d\n",pid);
        logFile("[INFO] Program I launched");
        list_pid[2]=pid;
    }

    // Open C
    pid = fork();
    if (pid < 0) { perror("fork C"); exit(EXIT_FAILURE); }
    if (pid == 0) {
        //execlp("konsole", "konsole", "-e", "./C", NULL);
        execlp("./bin/MultiObst/C", "C", NULL);
        perror("exec C");
        exit(EXIT_FAILURE);
    }
    else
    {
        fprintf(fp, "[C] start with pid %d\n",pid);
        logFile("[INFO] Program C launched");
        list_pid[3]=pid;
    }

//Close the file once every program have been launched
    if (fclose(fp) != 0) {
        logFile("[ERROR] Don't close PID_file.txt");
        perror("PID_file.txt close file");
        exit(1);
    }
//Wait for STOP message
    logFile("[INFO] Have launched all the programs");
    //Open fifo to get the "STOP" message from Input
    int fdC = open(fifoCtoI, O_RDONLY);
    if (fdC < 0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }
    //Read the fifo
    char buf[32] = {0};
    while(1){ //block until received a read message (a not if the pipe is closed)
        int a = read(fdC, buf, sizeof(buf));
        if(a!=0) break;
    }
    close(fdC);
    // Send SIGTERM to everyone
    logFile("[INFO] End of program, kill everyone");
    for (int i = 0; i < 3; i++) {
        kill(list_pid[i], SIGTERM);
    }

//Waiting for the programs to finish
    while (wait(NULL) > 0);
    logFile("[INFO] Every programs are finished");
//Finish cleanly
    logFile("[INFO] Finish cleanly");
    //Unlink every pipe
    unlink(fifoBtoD); 
    unlink(fifoDtoB); 
    unlink(fifoBtoI);
    unlink(fifoItoB);
    unlink(fifoCtoI);
    unlink(fifoBtoC);
    unlink(fifoCtoB);
    return 0;
}
