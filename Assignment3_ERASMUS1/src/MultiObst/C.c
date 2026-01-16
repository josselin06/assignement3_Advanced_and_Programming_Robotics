#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/file.h>
#include <signal.h>

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
    const char name[]="C";
    //print in the file
    fprintf(logfile, "[%s] [%s] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
}
int sockfd;
/* Print error message and exit */
void error(char *msg) {
    perror(msg);
    exit(0);
}
//function to write in the socket
void send_msg(const char *msg) {
    //Add "\n" to express the end of the message
    char buffer[4096];
    snprintf(buffer, 4096, "%s\n", msg);
    //Send the message
    int n = write(sockfd, buffer, strlen(buffer));
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
    while (i < 1023) {//sizeof(buf) - 1) {
        int n = read(sockfd, &c, 1);
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
    while (i < sizeof(buf) - 1) {
        int n = read(sockfd, &c, 1);
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
        printf("Response error: expected \"%s\", received \"%s\"\n", resp, buf);
        exit(1);
    }
}

int main() {
//Starting
    char text_pid[256];
    snprintf(text_pid, sizeof(text_pid),"[INFO] Program is starting with pid %d",getpid());
    logFile(text_pid); 
//Declare the variables
    // Socket parameters
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    //buffer
    char buffer[1024];
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
    //Creating pipe from Blackboard  to Client
    char * fifoBtoC = "/tmp/fifoBtoCa"; 
    //Creating pipe from Client to Blackboard  
    char * fifoCtoB = "/tmp/fifoCtoBa"; 
    //Creating pipe from Client to Init 
    char * fifoCtoI = "/tmp/fifoCtoIa";

    int fd1 = open(fifoCtoB, O_WRONLY);
    int fd2 = open(fifoBtoC, O_RDONLY);
    int fd3 = open(fifoCtoI, O_WRONLY);

    if (fd1 < 0 || fd2 < 0 || fd3 < 0) {
        logFile("[ERROR] Don't open fifo");
        perror("open fifo");
        exit(1);
    }
//Read the network parameter file
    char host_name[256];
    int port_server;

    char line[256];
    FILE *f = fopen("config/network_setup.txt", "r");
    if (!f) {
        logFile("[ERROR] Don't open the network file");
        perror("Cannot open network file");
        exit(1);
    }
    // Read the adress
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %255s", host_name);
    }
    // Read the port
    if (fgets(line, sizeof(line), f)) {
        sscanf(line, " %*[^=]= %d", &port_server);
    }

    fclose(f);
//Socket setup
    logFile("[INFO] Try to connect with server");
    // Create the socket
    portno = port_server;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        logFile("[ERROR] Don't open socket");
        error("ERROR opening socket");
    }
    // Set the server
    server = gethostbyname(host_name);
    if (server == NULL) {
        logFile("[ERROR] Don't find the host");
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
//connect
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        logFile("[ERROR] Don't connect to server");
        error("ERROR connecting");
    }   
//Receive the first information
    //protocole
    logFile("[INFO] Start speaking with the server");
    recv_resp("ok");
    send_msg("ook");
    recv_msg(buffer);
    sscanf(buffer, "size %d, %d", &l, &h);
    //printf("Received %s\n",buffer); //debug message----
    //printf("Size : l=%d, h=%d\n",l,h); //debug message----
    send_msg("sok");
    //Send size to the blackboard
    char msg[1024];
    int pos = snprintf(msg, sizeof(msg), "%d %d\n", l, h);
    write(fd1, msg, pos);
//main loop
    logFile("[INFO] Start routine communication");
    while(1) {
    //protocole
        recv_msg(buffer);
        //printf("Recieved : %s\n",buffer);
        
        if (strcmp(buffer, "drone") == 0) {
        //receive the drone coordinates for the socket
            recv_msg(buffer);
            sscanf(buffer, "%f, %f", &Xdrone, &Ydrone);
            //printf("Drone coordinates : %s\n",buffer); //debug message----
            //printf("Drone : X=%f, Y=%f\n",Xdrone,Ydrone); //debug message----
            Ydrone=h-Ydrone;
            send_msg("dok");
        //transmit them to the Blackboard
            //ask to write
            write(fd1, "w", 1);
            //send the values when the blackboard is ready
            n = read(fd2, buffer, sizeof(buffer));
            if (n > 0 && strncmp(buffer, "ok", 2) == 0) {
                char msg[1024];
                snprintf(msg, sizeof(msg), "%f %f\n", Xdrone, Ydrone);
                write(fd1, msg, sizeof(msg));
            }
        }
        
        if (strcmp(buffer, "obst") == 0) {
        //ask the obstacle coordinates to the Blackboard
            //ask for reading
            write(fd1, "r", 1);
            //read the values
            int n = read(fd2, buffer, sizeof(buffer));
            if (n > 0) {
                //values of the variables
                sscanf(buffer, "%f %f", &Xobst, &Yobst);
            }
        //transmit it to the socket
            Yobst=h-Yobst;
            //printf("Obst : X=%f, Y=%f\n",Xobst,Yobst); //debug message----
            snprintf(buffer, sizeof(buffer),"%d, %d",(int)Xobst, (int)Yobst); //SEND coordinates in INT
            //snprintf(buffer, sizeof(buffer),"%f, %f",Xobst, Yobst); //SEND coordinates in FLOAT
            send_msg(buffer);
            recv_resp("pok");
            //printf("Obst coordinates : %s\n",buffer); //debug message----
        }
    //case to quit
        if (strcmp(buffer, "q") == 0) {
            send_msg("qok");
            logFile("[INFO] Ask for the master process to finish the program");
            write(fd3,"STOP",4);
            break;
        }
        usleep(10000); //No need to be faster than D to communicate
    }
    //printf("End loop\n"); //debug message----
//Terminate the program
    logFile("[INFO] Finish cleanly");
    close(fd1);
    close(fd2);
    close(fd3);
    return 0;
}