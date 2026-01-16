#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/file.h>
#include <signal.h>

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
    const char name[]="S";
    //print in the file
    fprintf(logfile, "[%s] [%s] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}
int newsockfd;
/* Print error message and exit */
void error(char *msg) {
    perror(msg);
    exit(0);
}
//function to write in the socket
void send_msg(const char *msg) {
    //Add "\n" to express the end of the message
    char buffer[1024];
    snprintf(buffer, 1024, "%s\n", msg);
    //Send the message
    int n = write(newsockfd, buffer, strlen(buffer));
    if (n < 0) {
        logFile("[ERROR] Don't write in the socket");
        perror("ERROR writing to socket");
        exit(1);
    }
}
//function to receive a message
void recv_msg(char *buf) {
    //clear the buffer
    bzero(buf,1024);
    //Read the socket until a "\n" -> end of a message
    int i = 0;
    char c;
    while (i < 1024) {
        int n = read(newsockfd, &c, 1);
        if (n <= 0) {
            logFile("[ERROR] Don't read in the socket");
            perror("ERROR reading from socket");
            exit(1);
        }
        if (c == '\n')
            break;
        buf[i++] = c;
    }
    buf[i] = '\0';
}
//function to ckeck if it received a specific response
void recv_resp(const char *resp) {
    //Read the message
    char buf[1024];
    //Read the socket until a "\n" -> end of a message
    int i = 0;
    char c;
    while (i < 1024) {
        int n = read(newsockfd, &c, 1);
        if (n <= 0) {
            logFile("[ERROR] Don't read in the socket");
            perror("ERROR reading from socket");
            exit(1);
        }
        if (c == '\n')
            break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    //Compare the message with wath it is supposed to receive
    if (strcmp(buf, resp) != 0) {
        logFile("[ERROR] The socket response is not what expected");
        printf("Response error: expected \"%s\", received \"%s\"\n", resp, buf);
        exit(1);
    }
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
//Declare the varaiables
    //Socket parameters
    int sockfd, portno;
    socklen_t clilen;
    char buffer[1024];
    struct sockaddr_in serv_addr, cli_addr;
    //For the Drone
    float Xdrone, Ydrone;
    //For the Obstacle
    float Xobst, Yobst;
    //Window size
    int l,h;

    //Initializing some varaiables :
    Xdrone=0; Ydrone=0;
    Xobst=0; Yobst=0;
    l=0; h=0;
//Openning the pipe
    //Creating pipe from Blackboard  to Server
    char * fifoBtoS = "/tmp/fifoBtoS"; 
    //Creating pipe from Server to Blackboard  
    char * fifoStoB = "/tmp/fifoStoB"; 

    int fd1 = open(fifoStoB, O_WRONLY);
    int fd2 = open(fifoBtoS, O_RDONLY);

    if (fd1 < 0 || fd2 < 0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }
//Read the network parameter file
    int port_server;

    char line[256];
    FILE *f = fopen("config/network_setup.txt", "r");
    if (!f) {
        logFile("[ERROR] Don't open the network file");
        perror("Cannot open network file");
        exit(1);
    }
    // Ignore first line
    fgets(line, sizeof(line), f);
    // Read the port
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %d", &port_server);
    }

    fclose(f);
//Signal setup
    //handler_stop
    struct sigaction sa;
    sa.sa_handler = handler_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);
//Socket setup
    //Create the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logFile("[ERROR] Don't open the socket");
        error("ERROR opening socket");
    }
    // Set the server
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = port_server;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    // Bind
    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        logFile("[ERROR] Don't bind the socket");
        error("ERROR on binding");
    }
//Wait for a client
    logFile("[INFO] Waiting for a client");
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        logFile("[ERROR] Don't accept the client");
        error("ERROR on accept");
    }
//Send the first information
    //Protocole
    logFile("[INFO] Start speaking with the client");
    send_msg("ok");
    recv_resp("ook");
    //Ask the variables to the blackboard
    //ask for reading
    write(fd1, "r", 1);
    //read the values
    int n = read(fd2, buffer, sizeof(buffer));
    if (n > 0) {
        //values of the variables
        sscanf(buffer, "%d %d %f %f", &l, &h, &Xdrone, &Ydrone);
    }
    //Protocole
    snprintf(buffer, sizeof(buffer),"size %d, %d",l,h); // ----------------------------------------------------------------------------------------------protocole
    send_msg(buffer);
    //printf("Size : l=%d, h=%d\n",l,h); //-----debug message----
    //printf("Send : %s\n",buffer); //-----debug message----
    recv_resp("sok");
//main loop
    logFile("[INFO] Start routine communication");
    while(1) {
    //if end of loop -> tel to exit
        if(running==0) {
            logFile("[INFO] Tell the client to exit");
            printf("Must end\n");
            send_msg("q");
            recv_resp("qok");
            break;
        }
    //Ask the variables to the blackboard
        //ask for reading
        write(fd1, "r", 1);
        //read the values
        int n = read(fd2, buffer, sizeof(buffer));
        if (n > 0) {
            //values of the variables
            sscanf(buffer, "%d %d %f %f", &l, &h, &Xdrone, &Ydrone);
        }
    //Protocole
        send_msg("drone");
        //printf("Drone coordinates : %f, %f\n",Xdrone,h-Ydrone); //-----debug message----
        snprintf(buffer, sizeof(buffer), "%d, %d",(int)Xdrone,(int)(h-Ydrone)); //SEND coordinates in INT ------------------------------------------------protocole
        //snprintf(buffer, sizeof(buffer), "%f, %f",Xdrone,(h-Ydrone)); //SEND coordinates in FLOAT ------------------------------------------------------protocole
        //printf("Drone coordinates : %s\n",buffer); //----debug message----
        send_msg(buffer);
        recv_resp("dok");
        send_msg("obst");
        recv_msg(buffer);
        sscanf(buffer, "%f, %f", &Xobst, &Yobst);//-------------------------------------------------------------------------------------------------------protocole
        //printf("Obst coordinates : %s\n",buffer); //-----debug message----
        //printf("Obst coordinates : %f, %f\n",Xobst, Yobst); //-----debug message----
        Yobst=h-Yobst;
        send_msg("pok");
    //Send the obstacles coordinates to the Blackboard
        if(running) {
            //ask to write
            write(fd1, "w", 1);
            //send the values when the blackboard is ready
            n = read(fd2, buffer, sizeof(buffer));
            if (n > 0 && strncmp(buffer, "ok", 2) == 0) {
                char msg[1024];
                int pos = snprintf(msg, sizeof(msg), "%f %f\n", Xobst, Yobst);
                write(fd1, msg, pos);
            }
        }
    //
        usleep(10000); //No need to be faster than D to communicate
    }
    //printf("End loop\n"); //-----debug message----
//Terminate the program
    logFile("[INFO] Finish cleanly");
     close(sockfd);
     return 0; 
}