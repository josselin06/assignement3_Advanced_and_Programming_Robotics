#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
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
    const char name='D';
    //print in the file
    fprintf(logfile, "[%s] [%c] %s\n", time, name, text);
    //flush, unlock and close
    fflush(logfile);
    flock(fd, LOCK_UN);
    fclose(logfile);
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
//Declare the variables
    //From the param file
    float T; //Integration interval
    float M; //Mass of the drone
    float Rho; //viscous coefficient
    float repF; //repulse force
    float Fscale; //motor force scale
    int max_force;
    //for the drone
    float Xdrone, Ydrone; //Position
    float Xspeed, Yspeed; //Velocity
    float Xforce, Yforce; //motor force
    //From Input
    char key; //key pressed
    //for window
    int l, h;

//Openning the pipe
    //Creating pipe from Blackboard  to Drone
    char * fifoBtoD = "/tmp/fifoBtoDa"; 
    //Creating pipe from Drone to Blackboard  
    char * fifoDtoB = "/tmp/fifoDtoBa"; 

    int fd1 = open(fifoDtoB, O_WRONLY);
    int fd2 = open(fifoBtoD, O_RDONLY);

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
//main loop
    logFile("[INFO] Start calculating the dynamic parameters");
    while (running) {
    // Get the parameters
        //ask for reading
        write(fd1, "r", 1);

        //read the values
        char buffer[4096];
        int n = read(fd2, buffer, sizeof(buffer));
        if (n > 0) {
            //values of the variables
            sscanf(buffer, "%f %f %f %f %f %d %f %f %f %f %f %f %d %d %c",
            &T, &M, &Rho, &repF, &Fscale, &max_force, &Xdrone, &Ydrone, &Xspeed, &Yspeed, &Xforce, &Yforce, &l, &h, &key);
        }
    //
    //----------------Start calculation-------------------------------------------
        /*
        calcule of obstacles forces : 
        mesure distance between the drone and the obstacle
        obstacle radius = repulse force *3 (after this distance, the force can be negligeable)
        if(distance >= obstacle radius)
            force = 0
        else
            repulsive force= calculation of the force  with a decreasing exponencial curve, witha maximum at distance=0 ; force max = repulse force
        
        somme_of_obstacles_force = for Geofencing only
        */
        float somme_of_obstacles_force_X=0.0, //X and Y componant of the somme of the repulse force vectors of the obstacles
              somme_of_obstacles_force_Y=0.0;
        float repR = repF*3; //Repultion radius : after this distance, the force can be negligeable
        /*
        Geo fencing : 
        if the drone is to close to an edge, the force repulse it on the other side with the same logic than the obstacles
        */
        if(Xdrone<repR) //close to left
        {
            somme_of_obstacles_force_X += repF*exp(-Xdrone/repR);
        }
        if(Ydrone<repR) //close to top
        {
            somme_of_obstacles_force_Y += repF*exp(-Ydrone/repR);
        }
        if(Xdrone>(l-repR)) //close to right
        {
            somme_of_obstacles_force_X -= repF*exp(-(l-Xdrone)/repR);
        }
        if(Ydrone>(h-repR)) //close to bottom
        {
            somme_of_obstacles_force_Y -= repF*exp(-(h-Ydrone)/repR);
        }
        

        //New motor force, depend on the key pressed
        switch(key)
        {
            case 'z' :
            {
                Yforce -= Fscale;
                break;
            }
            case 'q' :
            {
                Xforce -= Fscale;
                break;
            }
            case 'd' :
            {
                Xforce += Fscale;
                break;
            }
            case 'x' :
            {
                Yforce += Fscale;
                break;
            }
            //----------------------------------------CORRECTED PART FROM 1ST ASSIGNMENT---------------
            case 's' :
            {
                Xforce=0; Yforce=0;
                Xspeed=0; Yspeed=0;
                break;
            }
            case 'e' :
            {
                Xforce=0; Yforce=0;
                break;
            }
            //------------------------------------------------------------------------------------------------
        }
        //Saturation of the force
        if(Xforce<-max_force) {Xforce=-max_force;}
        if(Xforce>max_force) {Xforce=max_force;}
        if(Yforce<-max_force) {Yforce=-max_force;}
        if(Yforce>max_force) {Yforce=max_force;}
        
        /*
        dynamic :
        Somme_of_forces (motors + obstacles)=M*a
        new_V = last_V + a*T
              = last_V + T/M * somme_of_forces
        Somme_of_forces = motor_force + viscous_force + Obstacle_forces
                        = new_motor_force  + (-K)*last_V  + Somme_of_obstacle_force
        
        new_X = last_X - newV*T
        */
        float somme_of_forces_X = Xforce - Rho*Xspeed + somme_of_obstacles_force_X;
        float somme_of_forces_Y = Yforce - Rho*Yspeed + somme_of_obstacles_force_Y;
        //----------------------------------------CORRECTED PART FROM 1ST ASSIGNMENT---------------
        if(key!='s') {
            Xspeed += T/M*somme_of_forces_X;
            Yspeed += T/M*somme_of_forces_Y;
        }
        //-----------------------------------------------------------------------------------------
        Xdrone += Xspeed*T;
        Ydrone += Yspeed*T;
        //Saturation of position
        if(Xdrone<0){Xdrone=0;}
        if(Ydrone<0){Ydrone=0;}
        if(Xdrone>l){Xdrone=l;}
        if(Ydrone>h){Ydrone=h;}

    //----------------End calculation---------------------------------------------
    // Send the new values to the blackboard
        //ask to write
        write(fd1, "w", 1);

        //send the values when the blackboard is ready
        n = read(fd2, buffer, sizeof(buffer));
        if (n > 0 && strncmp(buffer, "ok", 2) == 0) {
            char msg[1024];
            int pos = snprintf(msg, sizeof(msg), "%f %f %f %f %f %f\n", Xdrone, Ydrone, Xspeed, Yspeed, Xforce, Yforce);
            write(fd1, msg, pos);
        }
    //
        //Time period of the loop
        usleep((int)(T * 1e6));   // convert seconde → microseconde

    }
//Terminate program
    logFile("[INFO] Finish cleanly");
    close(fd1);
    close(fd2);
    return 0;
}
