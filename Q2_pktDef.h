#ifndef Q2_PKTDEF_H
#define Q2_PKTDEF_H

#include <stdio.h>
#include <sys/socket.h> //for socket(), connect(), send(), recv() functions
#include <arpa/inet.h> //different address structures are declared here
#include <stdlib.h> //atoi() which converts string to integer
#include <string.h>
#include <unistd.h> //close() function
#include <poll.h>   //for timer and retransmissions
#include <sys/types.h>  //for O_CREAT and all
#include <stdio.h> 
#include <fcntl.h>
#include <semaphore.h>  //for semaphores
#include <sys/shm.h>    //for shared memory
#include <errno.h>
#include <error.h>
#include <time.h>   //for time(NULL) which will be used to seed rand()
#include <sys/time.h>

#define PACKET_SIZE 100
#define BUFFERSIZE 10    //Indicates the size of the buffer in terms of data packets
#define PORT_RELAY1 8881    //PORT of the relay node 1
#define PORT_RELAY2 8882    //PORT of the relay node 2
#define PORT_SERVER 8883    //PORT of the server
#define PORT_CLIENT 8884
#define IP "127.0.0.1"  //IP address of the server process
#define TIMEOUT 1000 //Timeout (in ms)
#define MAXPENDING 10   //Maximum number of processes that can be waiting for connection from server 
                        //(just for formality, not needed in our case)
#define PDR 70  //Packet Drop Rate
#define MAX_ATTEMPTS 1000 //Maximum number of retransmission before leaving any hope of transmission
#define WINDOW_SIZE 4

typedef struct bufferEntry
{
    int isFilled;
    int sizeOfPayload;
    int isLastPkt;
    char payload[PACKET_SIZE];
} bufferEntry;

typedef struct pkt
{
    int sizeOfPayload;  //Size of the payload in bytes, 0 for ACK
    int seqNo;          //Sequence Number: First byte of the payload being transferred
    int isLackPkt;      //1 if its the last data/ACK pkt
    int isData;         //1 if the pkt is data pkt
    char src[10];       //Name of the src
    char dest[10];      //Name of the destination
    char payload[PACKET_SIZE];  //Memory for the payload
} PKT;

char * timeStamp();
#endif