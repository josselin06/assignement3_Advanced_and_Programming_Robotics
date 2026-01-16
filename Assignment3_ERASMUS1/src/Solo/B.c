#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <string.h>
#include <ncurses.h>
#include <signal.h>
#include <math.h>
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
    const char name='B';
    //print in the file
    fprintf(logfile, "[%s] [%c] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}
//function to read the config file and modify the corresponding variables
void read_config(float *T, float *M, float *Rho, float *repF, float *Fscale, int *max_force) {
    //open the file
    char line[256];
    FILE *f = fopen("config/config.txt", "r");
    if (!f) {
        perror("Cannot oppen config file");
        exit(1);
    }
    // Ignore first line
    fgets(line, sizeof(line), f);
    // Read T
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %f", T);
    }
    // Read M
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %f", M);
    }
    // Read Rho
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %f", Rho);
    }
    // Read repF
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %f", repF);
    }
    // Read Fscale
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %f", Fscale);
    }
    // Read max_force
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %d", max_force);
    }

    fclose(f);
}
//function to print a ractangle
void draw_rect(WINDOW *win, int y, int x, int w, int h) {
    // corners
    mvwaddch(win, y,     x,     ACS_ULCORNER);
    mvwaddch(win, y,     x+w-1, ACS_URCORNER);
    mvwaddch(win, y+h-1, x,     ACS_LLCORNER);
    mvwaddch(win, y+h-1, x+w-1, ACS_LRCORNER);
    // horizontal lines
    mvwhline(win, y,     x+1,     ACS_HLINE, w-2);
    mvwhline(win, y+h-1, x+1,     ACS_HLINE, w-2);
    // vertical lines
    mvwvline(win, y+1,   x,       ACS_VLINE, h-2);
    mvwvline(win, y+1,   x+w-1,   ACS_VLINE, h-2);
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
    char * fifoBtoW = "/tmp/fifoBtoW";
    int fdW = open(fifoBtoW, O_WRONLY);
    if (fdW < 0) {
        perror("open fifo");
        exit(1);
    }
//main loop
    while(running){
        write(fdW, (const void *)messagetowatchdog, sizeof(messagetowatchdog));
        kill(watchdog_pid,SIGRTMIN);
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
//-----Declare the variables of the blackboard
    //From the param file
    float T; //Integration interval
    float M; //Mass of the drone
    float Rho; //viscous coefficient
    float repF; //repulse force
    float Fscale; //motor force scale
    int max_force;
    //From Input
    char key; //key pressed
    //From Obstacles
    int const nbObscl=10; //number of simultaneous obstacles
    int Obstacles[2][nbObscl];
    //From Targets
    int Targets[2][9];
    //For the Drone
    float Xdrone, Ydrone;
    float Xspeed, Yspeed;
    float Xforce, Yforce;
    //For the score
    int Score;
    int ActualTarget[3][9];

    //Initializing some varaiables :
    Xdrone=50; Ydrone=20;
    Xspeed=0; Yspeed=0;
    Xforce=0; Yforce=0;
    for(int j = 0; j < 9; j++)
        ActualTarget[0][j] = 1;
    for(int j = 0; j < 9; j++)
        ActualTarget[1][j] = 0;
    for(int j = 0; j < 9; j++)
        ActualTarget[2][j] = 0;
    Score=0;
// -----OPENNING THE 4 PIPES-------------------------------------

    char * fifoBtoD = "/tmp/fifoBtoD"; 
    char * fifoDtoB = "/tmp/fifoDtoB"; 
    char * fifoBtoI = "/tmp/fifoBtoI"; 
    char * fifoItoB = "/tmp/fifoItoB"; 
    char * fifoBtoO = "/tmp/fifoBtoO"; 
    char * fifoOtoB = "/tmp/fifoOtoB"; 
    char * fifoBtoT = "/tmp/fifoBtoT"; 
    char * fifoTtoB = "/tmp/fifoTtoB"; 
    
    int fd_D_in  = open(fifoDtoB, O_RDONLY);
    int fd_D_out = open(fifoBtoD, O_WRONLY);

    int fd_I_in  = open(fifoItoB, O_RDONLY);
    int fd_I_out = open(fifoBtoI, O_WRONLY);

    int fd_O_in  = open(fifoOtoB, O_RDONLY);
    int fd_O_out = open(fifoBtoO, O_WRONLY);

    int fd_T_in  = open(fifoTtoB, O_RDONLY);
    int fd_T_out = open(fifoBtoT, O_WRONLY);
    
    if (fd_D_in < 0 || fd_D_out < 0 ||
        fd_I_in < 0 || fd_I_out < 0 ||
        fd_O_in < 0 || fd_O_out < 0 ||
        fd_T_in < 0 || fd_T_out < 0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }

// ncurses setup
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    int infoWidth = 70;
    WINDOW* win1 = newwin(maxY, maxX - infoWidth, 0, 0);
    WINDOW* win2 = newwin(maxY, infoWidth, 0, maxX - infoWidth);
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);
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
//--------Main loop with select()----------------------
    logFile("[INFO] Start sharing the paramerts and printing the scene");
    while (running) {
    //Update the constants from config.txt
        strcpy((char *)messagetowatchdog, "Update the constants from config.txt\n");
        read_config(&T, &M, &Rho, &repF, &Fscale, &max_force);

    //Create the select()
        strcpy((char *)messagetowatchdog, "Create the select()\n");
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(fd_D_in, &rset);
        FD_SET(fd_I_in, &rset);
        FD_SET(fd_O_in, &rset);
        FD_SET(fd_T_in, &rset);

        int maxfd = fd_D_in;
        if (fd_I_in > maxfd) maxfd = fd_I_in;
        if (fd_O_in > maxfd) maxfd = fd_O_in;
        if (fd_T_in > maxfd) maxfd = fd_T_in;
        
        select(maxfd + 1, &rset, NULL, NULL, NULL);

        if(running){
    //
    // if Drone is asking
        if (FD_ISSET(fd_D_in, &rset)) {
            strcpy((char *)messagetowatchdog, "Talking with Drone\n");
            char cmd;
            if (read(fd_D_in, &cmd, 1) > 0) {
                // if asking to read
                if (cmd == 'r') {
                    char buf[4096];
                    //Write the variables in the buff
                    int pos = snprintf(buf, sizeof(buf), "%f %f %f %f %f %d %f %f %f %f %f %f %c ",
                    T, M, Rho, repF, Fscale, max_force, Xdrone, Ydrone, Xspeed, Yspeed, Xforce, Yforce, key);
                    //Write the tab in the buff
                    for (int i = 0; i < 2; i++)
                    {
                        for (int j = 0; j < nbObscl; j++)
                        {
                            pos += snprintf(buf + pos, sizeof(buf) - pos, "%d ", Obstacles[i][j]);
                        }
                    }
                    buf[pos++] = '\n';
                    //send
                    write(fd_D_out, buf, pos);
                }
                //if asking to write
                else if (cmd == 'w') {
                    //send "ok"
                    write(fd_D_out, "ok\n", 3);

                    //read the values
                    char buf[1024];
                    if (read(fd_D_in, buf, sizeof(buf)) > 0) {
                        sscanf(buf, "%f %f %f %f %f %f", &Xdrone, &Ydrone, &Xspeed, &Yspeed, &Xforce, &Yforce);
                    }
                }
            }
        }
    // if Input is asking
        if (FD_ISSET(fd_I_in, &rset)) {
            strcpy((char *)messagetowatchdog, "Talking with Inputs\n");
            char cmd;
            if (read(fd_I_in, &cmd, 1) > 0) {
                //send "ok"
                write(fd_I_out, "ok\n", 3);

                //read the value
                char buf[128];
                if (read(fd_I_in, buf, sizeof(buf)) > 0) {
                    sscanf(buf, "%c", &key);
                }
            }
        }
    // if Obstacles is asking
        if (FD_ISSET(fd_O_in, &rset)) {
            strcpy((char *)messagetowatchdog, "Talking with Obstacles\n");
            char cmd;
            if (read(fd_O_in, &cmd, 1) > 0 && cmd == 'w') {
                //send "ok"
                write(fd_O_out, "ok\n", 3);

                //read the values of the tab
                char buf[1024];
                if (read(fd_O_in, buf, sizeof(buf)) > 0) {
                    char *p = buf;

                    for (int i = 0; i < 2; i++)
                    {
                        for (int j = 0; j < nbObscl; j++) {
                            Obstacles[i][j] = (int)strtol(p, &p, 10);  // base 10
                        }
                    }
                }
            }
        }
    // if Targets is asking
        if (FD_ISSET(fd_T_in, &rset)) {
            strcpy((char *)messagetowatchdog, "Talking with Targets\n");
            char cmd;
            if (read(fd_T_in, &cmd, 1) > 0 && cmd == 'w') {
                //send "ok"
                write(fd_T_out, "ok\n", 3);

                //read the values of the tab
                char buf[1024];
                if (read(fd_T_in, buf, sizeof(buf)) > 0) {
                    char *p = buf;

                    for (int i = 0; i < 2; i++)
                    {
                        for (int j = 0; j < 9; j++) {
                            Targets[i][j] = (int)strtol(p, &p, 10);  // base 10
                        }
                    }
                }
            }
        }
    //
        }
    
    //Calcule score
        strcpy((char *)messagetowatchdog, "Calcule score\n");
        //Check if actual target are reached (0 = not reached ; 1 = reached)
        for (int numero_next_target=0;numero_next_target<10;numero_next_target++)
        {
            if(numero_next_target==9) // case where all the targets have been reached 
            {
                //reset the tab
                for(int i=0;i<9;i++)
                {
                    ActualTarget[0][i]=0;
                }
                //put new coordinates
                for(int j=0;j<2;j++)
                {
                    for(int i =0;i<9;i++)
                    {
                        ActualTarget[j+1][i]=Targets[j][i];
                    }
                }
                break;
            }
            if(ActualTarget[0][numero_next_target]==0) //the next target to reach
            {
                float distance=sqrt((Xdrone-ActualTarget[1][numero_next_target])*(Xdrone-ActualTarget[1][numero_next_target])+(Ydrone-ActualTarget[2][numero_next_target])*(Ydrone-ActualTarget[2][numero_next_target])); //absolute distance between the target and the drone
                if(distance<1.5) // if the robot is close to the target, it is considered as "reached"
                {
                    Score++;
                    ActualTarget[0][numero_next_target]=1;
                }
                break;
            }
        }
    //

    //Print the scene
        strcpy((char *)messagetowatchdog, "Print the scene\n");
        werase(win1);
        werase(win2);
        //Window1 = simulation
        draw_rect(win1, 0,0,101, 41); //There was some bugs with the box function to I created mine.
        //----------------------------------------CORRECTED PART FROM 1ST ASSIGNMENT---------------
        //Print the Obstacles
        wattron(win1, COLOR_PAIR(1));
        for (int j = 0; j < nbObscl; j++) {
            mvwprintw(win1, Obstacles[1][j], Obstacles[0][j], "0");
        }
        wattroff(win1, COLOR_PAIR(1));
        //Print only the Targets not reached
        wattron(win1, COLOR_PAIR(2));
        for (int j = 0; j < 9; j++) {
            if(ActualTarget[0][j]==0)
                mvwprintw(win1, ActualTarget[2][j], ActualTarget[1][j], "%d",j+1);
        }
        wattroff(win1, COLOR_PAIR(2));
        //Print the drone
        wattron(win1, COLOR_PAIR(3));
        mvwprintw(win1, Ydrone, Xdrone, "X");
        wattroff(win1, COLOR_PAIR(3));
        //-------------------------------------------------------------------------------------------

        //Window2 = output
        box(win2, 0, 0);

        mvwprintw(win2, 2, 2, "PARAMS");
        mvwprintw(win2, 3, 2, "T = %f",T);
        mvwprintw(win2, 4, 2, "M = %f",M);
        mvwprintw(win2, 5, 2, "Rho = %f",Rho);
        mvwprintw(win2, 6, 2, "repF = %f",repF);
        mvwprintw(win2, 7, 2, "Fscale = %f",Fscale);
        mvwprintw(win2, 8, 2, "maxspeed = %d",max_force);
        mvwprintw(win2, 9, 2, "INPUTS");
        mvwprintw(win2, 10, 2, "key = %c",key);
        mvwprintw(win2, 12, 2, "OBSTACLES");
        mvwprintw(win2, 13, 2, " ");
        for (int j = 0; j < nbObscl; j++) {
            wprintw(win2,"[%d]", Obstacles[0][j]);
        }
        mvwprintw(win2, 14, 2, " ");
        for (int j = 0; j < nbObscl; j++) {
            wprintw(win2,"[%d]", Obstacles[1][j]);
        }
        mvwprintw(win2, 15, 2, "TARGETS");
        mvwprintw(win2, 16, 2, " ");
        for (int j = 0; j < 9; j++) {
            wprintw(win2,"[%d]", ActualTarget[1][j]);
        }
        mvwprintw(win2, 17, 2, " ");
        for (int j = 0; j < 9; j++) {
            wprintw(win2,"[%d]", ActualTarget[2][j]);
        }
        mvwprintw(win2, 19, 2, "DRONE");
        mvwprintw(win2, 20, 2, "Xdrone = %f",Xdrone);
        mvwprintw(win2, 21, 2, "Ydrone = %f",Ydrone);
        mvwprintw(win2, 22, 2, "Xspeed = %f",Xspeed);
        mvwprintw(win2, 23, 2, "Yspeed = %f",Yspeed);
        mvwprintw(win2, 24, 2, "Xforce = %f",Xforce);
        mvwprintw(win2, 25, 2, "Yforce = %f",Yforce);
        mvwprintw(win2, 27, 2, "Score = %d",Score);
        
        wrefresh(win1);
        wrefresh(win2);
    //
    }
//Terminate program
    strcpy((char *)messagetowatchdog, "Terminate program\n");
    logFile("[INFO] Finish cleanly");
    close(fd_D_in );
    close(fd_D_out);
    close(fd_I_in );
    close(fd_I_out);
    close(fd_O_in );
    close(fd_O_out);
    close(fd_T_in );
    close(fd_T_out);
    delwin(win1);
    delwin(win2);
    endwin();
    pthread_detach(watchdogThread);
    return 0;
}
