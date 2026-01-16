#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ncurses.h>
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
    const char name='I';
    //print in the file
    fprintf(logfile, "[%s] [%c] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}
//function to print keys
void draw_key(WINDOW *win, int y, int x, const char *label) {
    // corners
    mvwaddch(win, y,   x,   ACS_ULCORNER);
    mvwaddch(win, y,   x+4, ACS_URCORNER);
    mvwaddch(win, y+2, x,   ACS_LLCORNER);
    mvwaddch(win, y+2, x+4, ACS_LRCORNER);
    // horizontale lines
    mvwhline(win, y,   x+1, ACS_HLINE, 3);
    mvwhline(win, y+2, x+1, ACS_HLINE, 3);
    // verticale lines
    mvwvline(win, y+1, x,   ACS_VLINE, 1);
    mvwvline(win, y+1, x+4, ACS_VLINE, 1);
    // lettre
    mvwprintw(win, y+1, x+2, "%s", label);
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
    char * fifoItoW = "/tmp/fifoItoW";
    int fdW = open(fifoItoW, O_WRONLY);
    if (fdW < 0) {
        perror("open fifo");
        exit(1);
    }
//main loop
    while(running){
        write(fdW, (const void *)messagetowatchdog, sizeof(messagetowatchdog));
        kill(watchdog_pid,SIGRTMIN+2);
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
    //For keyboard
    char key = ' ';
//openning the pipe
    //Creating pipe from Blackboard  to Input
    char * fifoBtoI = "/tmp/fifoBtoI"; 
    //Creating pipe from Input to Blackboard  
    char * fifoItoB = "/tmp/fifoItoB"; 
    //Creating pipe from Input to Init 
    char * fifoItoI = "/tmp/fifoItoI";

    int fd1 = open(fifoItoB, O_WRONLY);
    int fd2 = open(fifoBtoI, O_RDONLY);
    int fd3 = open(fifoItoI, O_WRONLY);

    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }
// ncurses setup
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    WINDOW *win = newwin(20,21, 2, 0);
    nodelay(stdscr, TRUE);
    mvprintw(0,0,"Inputs : press S to start.");
//Signal setup
    //handler_stop
    struct sigaction sa;
    sa.sa_handler = handler_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
//Thread setup
    pthread_t watchdogThread;
    pthread_create(&watchdogThread, NULL, watchdog_thread, NULL);
    logFile("[INFO] Start talking with Watchdog");
//Main loop
    logFile("[INFO] Start reading the keyboard");
    while (running) {
    //Read the key pressed
        strcpy((char *)messagetowatchdog, "Read the key pressed\n");
        char ch;
        //wait for a key pressed until running = 0
        while(running)  {
            ch = getch();
            if(ch!=ERR) {
                break;
            }
        }
        //----------------------------------------CORRECTED PART FROM 1ST ASSIGNMENT---------------
        if ((ch=='a')||(ch=='z')||(ch=='q')||(ch=='s')||(ch=='d')||(ch=='x')||(ch=='e')) {
            key=ch;
        }
    //Print the informations on the screen
        strcpy((char *)messagetowatchdog, "Print the informations on the screen\n");
        werase(win);
        box(win, 0, 0);

        //print the keyboard in win
        draw_key(win, 1, 1, "A"); draw_key(win, 1, 6, "Z"); draw_key(win, 1, 11, "E");
        draw_key(win, 4, 3, "Q"); draw_key(win, 4, 8, "S"); draw_key(win, 4, 13, "D");
        draw_key(win, 7, 10, "X");
        //Informations abour the key's function
        mvwprintw(win, 9, 2,  "A : Exit");
        mvwprintw(win, 10, 2, "S : Brake");
        mvwprintw(win, 11, 2, "E : Neutral");
        mvwprintw(win, 12, 2, "Z : Up");
        mvwprintw(win, 13, 2, "Q : Left");
        mvwprintw(win, 14, 2, "D : Right");
        mvwprintw(win, 15, 2, "X : Down");
        //Print the key pressed
        mvwprintw(win, 17, 2, "Pressed key : %c", key);
        //-------------------------------------------------------------------------------------------

        wrefresh(win);
    //Transmit the key to the blackboard
        if(running) { //do not try to communicate ones the program is suppose to finish
            if(key=='a')//Case key = 'a' -> send message to master to close everything
            {
                strcpy((char *)messagetowatchdog, "Send message to master to close everything\n");
                logFile("[INFO] Ask for the master process to finish the program");
                write(fd3,"STOP",4);
            }
            else
            {
                strcpy((char *)messagetowatchdog, "Transmit the key to the blackboard\n");
                //ask to write
                write(fd1, "w", 1);
                //send the values when the blackboard is ready
                char buffer[128];
                int n = read(fd2, buffer, sizeof(buffer));
                if (n > 0 && strncmp(buffer, "ok", 2) == 0) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "%c\n", key);
                    write(fd1, msg, strlen(msg));
                }
            }
        }
    //
    }
//Terminate program
    strcpy((char *)messagetowatchdog, "Terminate program\n");
    logFile("[INFO] Finish cleanly");
    close(fd1);
    close(fd2);
    close(fd3);
    delwin(win);
    endwin();
    pthread_detach(watchdogThread);
    return 0;
//
}
