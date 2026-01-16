#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ncurses.h>
#include <sys/file.h>
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
    const char name='W';
    //print in the file
    fprintf(logfile, "[%s] [%c] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}
//function to print the time of the day
char * timeOfDay() {
    struct timeval tv;
    gettimeofday(&tv, NULL); //read seconds and microseconds
    struct tm* tm_info = localtime(&tv.tv_sec);//change seconds into hour/minut/second
    static char time[32];
    snprintf(time, sizeof(time),"%02d:%02d:%02d.%03d",tm_info->tm_hour,tm_info->tm_min,tm_info->tm_sec,(int)(tv.tv_usec/1000));
    return time;
}
//Signal handlers
volatile sig_atomic_t running = 1;
void handler_stop(int s) {running = 0;}
volatile sig_atomic_t ProgCall[5] = {0,0,0,0,0};//Tab to notify the watchdog of the signal received
void handlerB(int sig) {ProgCall[0]=1;}
void handlerD(int sig) {ProgCall[1]=1;}
void handlerI(int sig) {ProgCall[2]=1;}
void handlerO(int sig) {ProgCall[3]=1;}
void handlerT(int sig) {ProgCall[4]=1;}

int main(){
//Starting
    char text_pid[256];
    snprintf(text_pid, sizeof(text_pid),"[INFO] Program is starting with pid %d",getpid());
    logFile(text_pid);
//Declare the variables
    int loopNumber=0;//Count the number of loop of the watchdog
    int ProgSilent[5]={0,0,0,0,0};//Tab to notify if a signal is missing
    char bufB[256]="\0"; //to store the messages
    char bufD[256]="\0";
    char bufI[256]="\0";
    char bufO[256]="\0";
    char bufT[256]="\0";
//Openning the pipe
    char * fifoBtoW="/tmp/fifoBtoW"; //To watch B
    char * fifoDtoW="/tmp/fifoDtoW"; //To watch D
    char * fifoItoW="/tmp/fifoItoW"; //To watch I
    char * fifoOtoW="/tmp/fifoOtoW"; //To watch O
    char * fifoTtoW="/tmp/fifoTtoW"; //To watch T
    int fdB = open(fifoBtoW, O_RDWR);
    int fdD = open(fifoDtoW, O_RDWR);
    int fdI = open(fifoItoW, O_RDWR);
    int fdO = open(fifoOtoW, O_RDWR);
    int fdT = open(fifoTtoW, O_RDWR);
    if (fdB<0||fdD<0||fdI<0||fdO<0||fdT<0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }
//Signal setup
    //handler_stop
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;

    sa.sa_handler = handler_stop;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = handlerB;
    sigaction(SIGRTMIN, &sa, NULL);
    sa.sa_handler = handlerD;
    sigaction(SIGRTMIN+1, &sa, NULL);
    sa.sa_handler = handlerI;
    sigaction(SIGRTMIN+2, &sa, NULL);
    sa.sa_handler = handlerO;
    sigaction(SIGRTMIN+3, &sa, NULL);
    sa.sa_handler = handlerT;
    sigaction(SIGRTMIN+4, &sa, NULL);
// ncurses setup
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    WINDOW *win = newwin(15,70, 2, 0);
//Open a file to write the messages
    FILE *fp = fopen("log/Watchdog_file.txt", "w"); //Replace the past values
    if (fp == NULL) {
        logFile("[ERROR] Don't open Watchdog_file");
        perror("Watchdog_file.txt open file");
        exit(1);
    }
    setbuf(fp, NULL);
    fprintf(fp, "[%s] Watchdog is starting to receive the messages\n",timeOfDay());
//main loop
    logFile("[INFO] Start watching all the programs");
    while(running){
        loopNumber++;
    //Print the scene
        werase(win);
        box(win,0,0);
        mvwprintw(win,1,1,"Watchdog : loop n° %d",loopNumber);
        mvwprintw(win,3,1,"[B]");
        mvwprintw(win,5,1,"[D]");
        mvwprintw(win,7,1,"[I]");
        mvwprintw(win,9,1,"[O]");
        mvwprintw(win,11,1,"[T]");
    //Print the messages
        //For B
        if(ProgCall[0]) { //if the signal is received
            //read the message
            if (read(fdB, bufB, sizeof(bufB)) > 0) {
                mvwprintw(win,3,5,"%s",bufB);
                fprintf(fp,"[%s] [B] %s",timeOfDay(),bufB);
            }
            ProgCall[0]=0;
            ProgSilent[0]=0;
        }
        else { //if the signal is missing
            ProgSilent[0]++;
            if(ProgSilent[0]>5) { //alerte after several missing in a raw
                mvwprintw(win,3,5,"Silent for %d loop  ----ALERTE----",ProgSilent[0]);
                if(ProgSilent[0]==6) { //write the alerte in the file only once
                    fprintf(fp,"[%s] [B] ----ALERTE---- Program silent\n",timeOfDay());
                }
            }
            else { //while checking if the missing is permanent, print the old message
                mvwprintw(win,3,5,"%s",bufB);
            }
        }
        //For D
        if(ProgCall[1]) { //if the signal is received
            //read the message
            if (read(fdD, bufD, sizeof(bufD)) > 0) {
                mvwprintw(win,5,5,"%s",bufD);
                fprintf(fp,"[%s] [D] %s",timeOfDay(),bufD);
            }
            ProgCall[1]=0;
            ProgSilent[1]=0;
        }
        else { //if the signal is missing
            ProgSilent[1]++;
            if(ProgSilent[1]>5) { //alerte after several missing in a raw
                mvwprintw(win,5,5,"Silent for %d loop  ----ALERTE----",ProgSilent[1]);
                if(ProgSilent[1]==6) { //write the alerte in the file only once
                    fprintf(fp,"[%s] [D] ----ALERTE---- Program silent\n",timeOfDay());
                }
            }
            else { //while checking if the missing is permanent, print the old message
                mvwprintw(win,5,5,"%s",bufD);
            }
        }
        //For I
        if(ProgCall[2]) { //if the signal is received
            //read the message
            if (read(fdI, bufI, sizeof(bufI)) > 0) {
                mvwprintw(win,7,5,"%s",bufI);
                fprintf(fp,"[%s] [I] %s",timeOfDay(),bufI);
            }
            ProgCall[2]=0;
            ProgSilent[2]=0;
        }
        else { //if the signal is missing
            ProgSilent[2]++;
            if(ProgSilent[2]>5) { //alerte after several missing in a raw
                mvwprintw(win,7,5,"Silent for %d loop  ----ALERTE----",ProgSilent[2]);
                if(ProgSilent[2]==6) { //write the alerte in the file only once
                    fprintf(fp,"[%s] [I] ----ALERTE---- Program silent\n",timeOfDay());
                }
            }
            else { //while checking if the missing is permanent, print the old message
                mvwprintw(win,7,5,"%s",bufI);
            }
        }
        //For O
        if(ProgCall[3]) { //if the signal is received
            //read the message
            if (read(fdO, bufO, sizeof(bufO)) > 0) {
                mvwprintw(win,9,5,"%s",bufO);
                fprintf(fp,"[%s] [O] %s",timeOfDay(),bufO);
            }
            ProgCall[3]=0;
            ProgSilent[3]=0;
        }
        else { //if the signal is missing
            ProgSilent[3]++;
            if(ProgSilent[3]>5) { //alerte after several missing in a raw
                mvwprintw(win,9,5,"Silent for %d loop  ----ALERTE----",ProgSilent[3]);
                if(ProgSilent[3]==6) { //write the alerte in the file only once
                    fprintf(fp,"[%s] [O] ----ALERTE---- Program silent\n",timeOfDay());
                }
            }
            else { //while checking if the missing is permanent, print the old message
                mvwprintw(win,9,5,"%s",bufO);
            }
        }
        //For T
        if(ProgCall[4]) { //if the signal is received
            //read the message
            if (read(fdT, bufT, sizeof(bufT)) > 0) {
                mvwprintw(win,11,5,"%s",bufT);
                fprintf(fp,"[%s] [T] %s",timeOfDay(),bufT);
            }
            ProgCall[4]=0;
            ProgSilent[4]=0;
        }
        else { //if the signal is missing
            ProgSilent[4]++;
            if(ProgSilent[4]>5) { //alerte after several missing in a raw
                mvwprintw(win,11,5,"Silent for %d loop  ----ALERTE----",ProgSilent[4]);
                if(ProgSilent[4]==6) { //write the alerte in the file only once
                    fprintf(fp,"[%s] [T] ----ALERTE---- Program silent\n",timeOfDay());
                }
            }
            else { //while checking if the missing is permanent, print the old message
                mvwprintw(win,11,5,"%s",bufT);
            }
        }
    //
        mvwprintw(win,14,1," ");
        wrefresh(win);
        usleep(100000); //0,1s -> twice the frequence of the program talking, so it won't miss any message
    }
//Terminate program
    logFile("[INFO] Finish cleanly");
    close(fdB);
    close(fdD);
    close(fdI);
    close(fdO);
    close(fdT);
    fprintf(fp,"[%s] Watchdog finishing\n",timeOfDay());
    if (fclose(fp) != 0) {
        logFile("[ERROR] Don't close Watchdog_file");
        perror("Watchdog_file.txt close file");
        exit(1);
    }
    delwin(win);
    endwin();
    return 0;
}