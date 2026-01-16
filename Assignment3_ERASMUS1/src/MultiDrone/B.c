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
    //For the Drone
    float Xdrone, Ydrone;
    float Xspeed, Yspeed;
    float Xforce, Yforce;
    //For the Obstacle
    float Xobst, Yobst;
    //Window size
    int l,h;

    //Initializing some varaiables :
    Xdrone=50; Ydrone=20;
    Xspeed=0; Yspeed=0;
    Xforce=0; Yforce=0;
    Xobst=0; Yobst=0;
    l=100; h=40;
// -----OPENNING THE 3 PIPES-------------------------------------

    char * fifoBtoD = "/tmp/fifoBtoD"; 
    char * fifoDtoB = "/tmp/fifoDtoB"; 
    char * fifoBtoI = "/tmp/fifoBtoI"; 
    char * fifoItoB = "/tmp/fifoItoB";
    char * fifoBtoS = "/tmp/fifoBtoS"; 
    char * fifoStoB = "/tmp/fifoStoB";
    
    int fd_D_in  = open(fifoDtoB, O_RDONLY);
    int fd_D_out = open(fifoBtoD, O_WRONLY);

    int fd_I_in  = open(fifoItoB, O_RDONLY);
    int fd_I_out = open(fifoBtoI, O_WRONLY);
    
    int fd_S_in  = open(fifoStoB, O_RDONLY);
    int fd_S_out = open(fifoBtoS, O_WRONLY);

    if (fd_D_in < 0 || fd_D_out < 0 ||
        fd_I_in < 0 || fd_I_out < 0 ||
        fd_S_in < 0 || fd_S_out < 0) {
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
//--------Main loop with select()----------------------
    logFile("[INFO] Start sharing the paramerts and printing the scene");
    while (running) {
    //Update the constants from config.txt
        read_config(&T, &M, &Rho, &repF, &Fscale, &max_force);

    //Create the select()
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(fd_D_in, &rset);
        FD_SET(fd_I_in, &rset);
        FD_SET(fd_S_in, &rset);

        int maxfd = fd_D_in;
        if (fd_I_in > maxfd) maxfd = fd_I_in;
        if (fd_S_in > maxfd) maxfd = fd_S_in;
        
        select(maxfd + 1, &rset, NULL, NULL, NULL);

        if(running){
    //
    // if Drone is asking
        if (FD_ISSET(fd_D_in, &rset)) {
            char cmd;
            if (read(fd_D_in, &cmd, 1) > 0) {
                // if asking to read
                if (cmd == 'r') {
                    char buf[4096];
                    //Write the variables in the buff
                    int pos = snprintf(buf, sizeof(buf), "%f %f %f %f %f %d %f %f %f %f %f %f %c %f %f",
                    T, M, Rho, repF, Fscale, max_force, Xdrone, Ydrone, Xspeed, Yspeed, Xforce, Yforce, key, Xobst, Yobst);
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
    // if Server is asking
        if (FD_ISSET(fd_S_in, &rset)) {
            char cmd;
            if (read(fd_S_in, &cmd, 1) > 0) {
                // if asking to read
                if (cmd == 'r') {
                    char buf[4096];
                    //Write the variables in the buff
                    snprintf(buf, sizeof(buf), "%d %d %f %f", l, h, Xdrone, Ydrone);
                    //send
                    write(fd_S_out, buf, strlen(buf));
                }
                //if asking to write
                else if (cmd == 'w') {
                    //send "ok"
                    write(fd_S_out, "ok\n", 3);

                    //read the values
                    char buf[1024];
                    if (read(fd_S_in, buf, sizeof(buf)) > 0) {
                        sscanf(buf, "%f %f", &Xobst, &Yobst);
                    }
                }
            }
        }
    //
        }
    //Print the scene
        werase(win1);
        werase(win2);
        //Window1 = simulation
        draw_rect(win1, 0,0,l+1,h+1); //There was some bugs with the box function so I created mine.
        //----------------------------------------CORRECTED PART FROM 1ST ASSIGNMENT---------------
        //Print the Obstacles
        wattron(win1, COLOR_PAIR(1));
        mvwprintw(win1, Yobst, Xobst, "O");
        wattroff(win1, COLOR_PAIR(1));
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

        mvwprintw(win2, 11, 2, "DRONE");
        mvwprintw(win2, 12, 2, "Xdrone = %f",Xdrone);
        mvwprintw(win2, 13, 2, "Ydrone = %f",Ydrone);
        mvwprintw(win2, 14, 2, "Xspeed = %f",Xspeed);
        mvwprintw(win2, 15, 2, "Yspeed = %f",Yspeed);
        mvwprintw(win2, 16, 2, "Xforce = %f",Xforce);
        mvwprintw(win2, 17, 2, "Yforce = %f",Yforce);
        mvwprintw(win2, 18, 2, "OBSTACLE");
        mvwprintw(win2, 19, 2, "Xobst = %f",Xobst);
        mvwprintw(win2, 20, 2, "Yobst = %f",Yobst);
        
        wrefresh(win1);
        wrefresh(win2);
    //
    }
//Terminate program
    logFile("[INFO] Finish cleanly");
    close(fd_D_in );
    close(fd_D_out);
    close(fd_I_in );
    close(fd_I_out);
    delwin(win1);
    delwin(win2);
    endwin();
    return 0;
}
