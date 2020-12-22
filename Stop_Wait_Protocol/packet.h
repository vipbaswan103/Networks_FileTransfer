#ifndef PACKET_H_
#define PACKET_H_

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
#include <netinet/in.h> 
#include <signal.h> 
#include <sys/select.h>

#define PACKET_SIZE 100  //Size of the packet
#define BUFSIZE 5    //Indicates the size of the buffer in terms of data packets
#define PORT 8880   //PORT of the server process
#define IP "127.0.0.1"  //IP address of the server process
#define TIMEOUT 100 //Timeout (in ms)
#define PDR 10  //Packet Drop Rate

#define MAXPENDING 10   //Maximum number of processes that can be waiting for connection from server 
                        //(just for formality, not needed in our case). 
#define MAX_ATTEMPTS 1000 //Maximum number of retransmission before leaving any hope of transmission

// #define FTOK_KEY1 48
// #define FTOK_KEY2 24
//The structure of packet

typedef struct buffEntry
{
    int isFilled;       //Indicates that payload containts valid data if set
    int sizeOfPayload;  //Size of actual data in the payload (becuase last pkt may not consume full PACKET_SIZE)
    int seqNo;          //Since seq no indicates the offset of the first byte of the packet in the actual file, it helps in writing to output.txt
    char payload[PACKET_SIZE];
} bufferEntry;

typedef struct __attribute__((packed)) pkt
{
    int sizeOfPayload;  //Size of the payload being transferred (in bytes) if pkt is DATA pkt, else set to 0
    int seqNo;          //Sequence no of the pkt
    int isLastPkt;      //Set if the pkt is the last DATA/ACK pkt
    int isData;         //Set if the pkt is a DATA pkt
    int channelID;      //Indicates the channel through which data/ACK is transferred
    char payload[PACKET_SIZE];  //Memory for the payload
} PKT;

#endif