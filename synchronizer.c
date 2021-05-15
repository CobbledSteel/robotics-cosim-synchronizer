/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
 * modified from: https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/tcpserver.c
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h> 

#include "/home/ubuntu/gazebo-cosim/packet.h"

#define BUFSIZE 1024

#define CYCLE_STEP 1000
#ifdef  MIDAS
#define MAX_SIM_CYCLES 1000*100
#else
//#define MAX_SIM_CYCLES 1000000*100
#define MAX_SIM_CYCLES 1000*100
#endif

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv) {
    int gazebo_parentfd; /* parent socket */
    int gazebofd; /* child socket */
    int gazebo_portno; /* port to listen on */
    int gazebo_clientlen; /* byte size of client's address */
    struct sockaddr_in gazebo_serveraddr; /* server's addr */
    struct sockaddr_in gazebo_clientaddr; /* client addr */
    struct hostent *gazebo_hostp; /* client host info */
    char buf[BUFSIZE]; /* message buffer */
    char *gazebo_hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */

    int firesim_parentfd; /* parent socket */
    int firesimfd; /* child socket */
    int firesim_portno; /* port to listen on */
    int firesim_clientlen; /* byte size of client's address */
    struct sockaddr_in firesim_serveraddr; /* server's addr */
    struct sockaddr_in firesim_clientaddr; /* client addr */
    struct hostent *firesim_hostp; /* client host info */
    char *firesim_hostaddrp; /* dotted decimal host addr string */

    // Synchronization variables
    uint64_t min_tick = 1;
    uint64_t min_cycle = 0;
    uint32_t cycle_step = 1000;
    uint32_t iterations = 0;
    uint32_t firesim_requests = 0;

    // Time measurement variables:
    struct timeval start, end;
    struct timeval start_temp, end_temp;
    long secs_used,micros_used;

    /* 
     * check command line arguments 
     */
    if (argc != 4) {
        fprintf(stderr, "usage: %s <gazebo port> <firesim port> <cycle step size>\n", argv[0]);
        exit(1);
    }
    gazebo_portno = atoi(argv[1]);
    firesim_portno = atoi(argv[2]);
    cycle_step = atoi(argv[3]);

    /* 
     * socket: create the parent socket 
     */
    gazebo_parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (gazebo_parentfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(gazebo_parentfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &gazebo_serveraddr, sizeof(gazebo_serveraddr));

    /* this is an Internet address */
    gazebo_serveraddr.sin_family = AF_INET;

    /* let the system figure out our IP address */
    gazebo_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* this is the port we will listen on */
    gazebo_serveraddr.sin_port = htons((unsigned short)gazebo_portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(gazebo_parentfd, (struct sockaddr *) &gazebo_serveraddr, 
                sizeof(gazebo_serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * listen: make this socket ready to accept connection requests 
     */
    if (listen(gazebo_parentfd, 5) < 0) /* allow 5 requests to queue up */ 
        error("ERROR on listen");

    /* 
     * socket: create the parent socket 
     */
    firesim_parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (firesim_parentfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(firesim_parentfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &firesim_serveraddr, sizeof(firesim_serveraddr));

    /* this is an Internet address */
    firesim_serveraddr.sin_family = AF_INET;

    /* let the system figure out our IP address */
    firesim_serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* this is the port we will listen on */
    firesim_serveraddr.sin_port = htons((unsigned short)firesim_portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(firesim_parentfd, (struct sockaddr *) &firesim_serveraddr, 
                sizeof(firesim_serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * listen: make this socket ready to accept connection requests 
     */
    if (listen(firesim_parentfd, 5) < 0) /* allow 5 requests to queue up */ 
        error("ERROR on listen");

    printf("Starting synchronization loop...\n");
    /* 
     * main loop: wait for a connection request, echo input line, 
     * then close connection.
     */
    gazebo_clientlen = sizeof(gazebo_clientaddr);
    firesim_clientlen = sizeof(firesim_clientaddr);

    printf("Connecting to gazebo...\n");
    /* 
     * accept: wait for a connection request 
     */
    gazebofd = accept(gazebo_parentfd, (struct sockaddr *) &gazebo_clientaddr, &gazebo_clientlen);
    if (gazebofd < 0) 
        error("ERROR on accept");

    /* 
     * gethostbyaddr: determine who sent the message 
     */
    gazebo_hostp = gethostbyaddr((const char *)&gazebo_clientaddr.sin_addr.s_addr, 
            sizeof(gazebo_clientaddr.sin_addr.s_addr), AF_INET);
    if (gazebo_hostp == NULL)
        error("ERROR on gethostbyaddr");
    gazebo_hostaddrp = inet_ntoa(gazebo_clientaddr.sin_addr);
    if (gazebo_hostaddrp == NULL)
        error("ERROR on inet_ntoa\n");
    printf("server established gazebo connection with %s (%s)\n", 
            gazebo_hostp->h_name, gazebo_hostaddrp);


    printf("Connecting to firesim...\n");
    /* 
     * accept: wait for a connection request 
     */
    firesimfd = accept(firesim_parentfd, (struct sockaddr *) &firesim_clientaddr, &firesim_clientlen);
    if (firesimfd < 0) 
        error("ERROR on accept");

    /* 
    /* 
     * gethostbyaddr: determine who sent the message 
     */
    firesim_hostp = gethostbyaddr((const char *)&firesim_clientaddr.sin_addr.s_addr, 
            sizeof(firesim_clientaddr.sin_addr.s_addr), AF_INET);
    if (firesim_hostp == NULL)
        error("ERROR on gethostbyaddr");
    firesim_hostaddrp = inet_ntoa(firesim_clientaddr.sin_addr);
    if (firesim_hostaddrp == NULL)
        error("ERROR on inet_ntoa\n");
    printf("server established firesim connection with %s (%s)\n", 
            firesim_hostp->h_name, firesim_hostaddrp);

    // Write step size to firesim
    printf("Writing cycle step size to firesim...\n");
    bzero(buf, BUFSIZE);
    buf[0] = CS_DEFINE_STEP;
    sprintf(buf + 1, "%011d", cycle_step);
    n = write(firesimfd, buf, strlen(buf));
    printf("Wrote %d bytes\n", n);

    // Perform initial firesim synchronization
    printf("Performing initial firesim synchronization...\n");
    do {
        bzero(buf, BUFSIZE);
        buf[0] = CS_REQ_CYCLES;
        n = write(firesimfd, buf, strlen(buf));
        bzero(buf, BUFSIZE);
        n = read (firesimfd, buf, BUFSIZE);
        //printf("Cycles: %d, %d\n", min_cycle - atoi(buf), min_cycle);
    } while (atoi(buf) >  0);

    printf("Starting main loop\n");

    // Start recording time
    gettimeofday(&start, NULL);
    /* 
     * write: echo the input string back to the client 
     */
    while(min_cycle <= MAX_SIM_CYCLES) {
        //printf("Sending gazebo msg...\n");
        //sprintf(buf, "%c", CS_GRANT_TOKEN);
        // Request cycle count
        //do {
        //printf("Measuring Gazebo token poll\n");
        //gettimeofday(&start_temp, NULL);

        bzero(buf, BUFSIZE);
        buf[0] = CS_REQ_CYCLES;
        n = write(gazebofd, buf, strlen(buf));
        //usleep(100000);
        bzero(buf, BUFSIZE);
        n = read (gazebofd, buf, BUFSIZE);

        //gettimeofday(&end_temp, NULL);
        //secs_used=(end_temp.tv_sec - start_temp.tv_sec); //avoid overflow by subtracting first
        //micros_used= ((secs_used*1000000) + end_temp.tv_usec) - (start_temp.tv_usec);
        //printf("%f\n",(float) micros_used / 1000);

        //printf("Ticks: %d, %d\n", atoi(buf), min_tick);
        //} while (atoi(buf) < min_tick);
        bzero(buf, BUFSIZE);
        min_tick++;

        //printf("Measuring Gazebo token allocaiton\n");
        //gettimeofday(&start_temp, NULL);

        // Grant token to increment cycles
        buf[0] = CS_GRANT_TOKEN;
        n = write(gazebofd, buf, strlen(buf));
        if (n < 0) 
            error("ERROR writing to socket");

        //gettimeofday(&end_temp, NULL);
        //secs_used=(end_temp.tv_sec - start_temp.tv_sec); //avoid overflow by subtracting first
        //micros_used= ((secs_used*1000000) + end_temp.tv_usec) - (start_temp.tv_usec);
        //printf("%f\n",(float) micros_used / 1000);

        //printf("Sending firesim msg...\n");
        //sprintf(buf, "%c", CS_GRANT_TOKEN);
        // Request cycle count
        iterations = 0;
        do {
        //printf("Measuring Firesim token poll\n");
        //gettimeofday(&start_temp, NULL);

            bzero(buf, BUFSIZE);
            buf[0] = CS_REQ_CYCLES;
            n = write(firesimfd, buf, strlen(buf));
            bzero(buf, BUFSIZE);
            iterations++;
            firesim_requests++;
            n = read (firesimfd, buf, BUFSIZE);
            //printf("Cycles: %d, %d\n", min_cycle - atoi(buf), min_cycle);

        //gettimeofday(&end_temp, NULL);
        //secs_used=(end_temp.tv_sec - start_temp.tv_sec); //avoid overflow by subtracting first
        //micros_used= ((secs_used*1000000) + end_temp.tv_usec) - (start_temp.tv_usec);
        //printf("%f\n",(float) micros_used / 1000);
        } while (atoi(buf) >  0);


        //printf("Measuring Firesim token allocation \n");
        //gettimeofday(&start_temp, NULL);

        //printf("Iterations: %d\n", iterations);
        bzero(buf, BUFSIZE);
        min_cycle += cycle_step;
        // Grant token to increment cycles
        buf[0] = CS_GRANT_TOKEN;
        n = write(firesimfd, buf, strlen(buf));
        if (n < 0) 
            error("ERROR writing to socket");
        //printf("Completed syncrhonization iteration!\n");

        //gettimeofday(&end_temp, NULL);
        //secs_used=(end_temp.tv_sec - start_temp.tv_sec); //avoid overflow by subtracting first
        //micros_used= ((secs_used*1000000) + end_temp.tv_usec) - (start_temp.tv_usec);
        //printf("%f\n",(float) micros_used / 1000);
    }
    // End recording time
    gettimeofday(&end, NULL);

    secs_used=(end.tv_sec - start.tv_sec); //avoid overflow by subtracting first
    micros_used= ((secs_used*1000000) + end.tv_usec) - (start.tv_usec);

    printf("Total Gazebo Ticks: %d\n", min_tick);
    printf("Total Firesim Cycles: %d\n", min_cycle);
    printf("Total Firesim Sync Requests: %d\n", firesim_requests);
    printf("Average Firesim Sync Requests: %2f\n", (float) firesim_requests / (min_tick));
    printf("micros: %d\n",micros_used);
    printf("millis: %f\n",(float) micros_used / 1000);
    printf("secs: %f\n",(float) micros_used / 1000000);
    close(gazebofd);
    close(firesimfd);
}

