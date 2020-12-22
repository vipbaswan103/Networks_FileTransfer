#include "packet.h"

void die(char * error)
{
    perror(error);
    exit(-1);
}
char * timeStamp()
{
    char * stamp = (char *)malloc(sizeof(char)*20);
    struct tm * timePtr;
    struct timeval tv;
    time_t currTime = time(NULL);
    timePtr = localtime(&currTime);
    gettimeofday(&tv, NULL);
    int tmp = strftime(stamp, 20, "%H:%M:%S", timePtr);

    char milliSec[8];
    sprintf(milliSec, ".%06ld", tv.tv_usec);
    strcat(stamp, milliSec);
    return stamp;
}

//For making a deep copy of the pkt (may be needed later for retransmissions)
void makeCopy(PKT * cpy, PKT sndPkt)
{
    cpy->isData = sndPkt.isData;
    strcpy(cpy->dest, sndPkt.dest);
    strcpy(cpy->src, sndPkt.src);
    cpy->isLackPkt = sndPkt.isLackPkt;
    strcpy(cpy->payload, sndPkt.payload);
    cpy->seqNo = sndPkt.seqNo;
    cpy->sizeOfPayload = sndPkt.sizeOfPayload;
}

//This function return 1, if the pkt should be dropped. Else 0 is returned
//randNum: It is a random number between 0 to 99 (both inclusive)
int testForDrop(int randNum)
{
    if(randNum >= 0 && randNum < PDR)
        return 1;
    return 0;
}

int main(int argc, char * argv[])
{
    if(argc != 2)
    {
        printf("Wrong number of args!!");
        exit(0);
    }
    //Seed to generate random numbers
    srand(time(NULL));

    //Declare various vars
    int skt, slen, slenR;
    struct sockaddr_in si_server, si_client, si_me, si_rcv;
    slen = sizeof(si_me);
    slenR = sizeof(si_rcv);
    memset((char *)&si_server, 0, slen);
    memset((char *)&si_me, 0, slen);
    memset((char *)&si_client, 0, slen);
    memset((char *)&si_rcv, 0, slenR);


    //Get appropriate address in si_me based on command line args
    si_me.sin_addr.s_addr = inet_addr(IP);
    if(atoi(argv[1]) == 1)
        si_me.sin_port = htons(PORT_RELAY1);
    else if(atoi(argv[1]) == 2)
        si_me.sin_port = htons(PORT_RELAY2);
    si_me.sin_family = AF_INET;

    //Address of the server
    si_server.sin_addr.s_addr = inet_addr(IP);
    si_server.sin_port = htons(PORT_SERVER);
    si_server.sin_family = AF_INET;

    //Address of the client
    si_client.sin_addr.s_addr = inet_addr(IP);
    si_client.sin_port = htons(PORT_CLIENT);
    si_client.sin_family = AF_INET;

    if((skt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("socket()");
    
    if(bind(skt, (struct sockaddr*)&si_me, slen) < 0)
        die("bind()");
    
    PKT rcvPkt, cpyRcvPkt;
    int bytesSent, bytesRcvd, randNum;
    char * stamp;

    FILE * fp;
    if(atoi(argv[1]) == 1)
        fp = fopen("log_relay1.txt", "w");
    else if(atoi(argv[1]) == 2)
        fp = fopen("log_relay2.txt", "w");
    
    if(fp == NULL)
        die("fopen()");
    while(1)
    {
        //Wait for data/ack pkt
        memset((char *)&rcvPkt, 0, sizeof(rcvPkt));
        bytesRcvd = recvfrom(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr *)&si_rcv, &slenR);
        if(bytesRcvd <= 0)
            die("recvfrom()");

        //Get the recv timestamp
        stamp = timeStamp();

        //Generate a random number
        randNum = rand()%100;

        //If data pkt, check whether to drop or not
        if(testForDrop(randNum) && rcvPkt.isData == 1)
        {
            if(atoi(argv[1]) == 1)
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "D", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "D", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            fflush(fp);
            if(atoi(argv[1]) == 1)
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "D", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "D", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            continue;
        }

        //If pkt isn't dropped, put the log
        if(atoi(argv[1]) == 1)
        {
            if(rcvPkt.isData == 1)
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", stamp, "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            fflush(fp);
            if(rcvPkt.isData == 1)
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", stamp, "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            strcpy(rcvPkt.src, "RELAY1\0");
        }
        else if(atoi(argv[1]) == 2)
        {
            if(rcvPkt.isData == 1)
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "R", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            else
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "R", stamp, "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            fflush(fp);
            if(rcvPkt.isData == 1)
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "R", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            else
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "R", stamp, "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            strcpy(rcvPkt.src, "RELAY2\0");
        }

        //Now, give delay to the pkt by making the process sleep
        randNum = rand()%3;
        usleep(randNum);

        makeCopy(&cpyRcvPkt, rcvPkt);

        //Route the pkt to appropriate next hop based on dest address in the received pkt
        if(!strcmp(rcvPkt.dest, "CLIENT\0"))
            bytesSent = sendto(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr*)&si_client, slen);
        else if(!strcmp(rcvPkt.dest, "SERVER\0"))
            bytesSent = sendto(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr*)&si_server, slen);
        
        if(bytesSent < 0)
            die("send()");

        //Get the send timestamp
        stamp = timeStamp();

        //Add the event to the log
        if(cpyRcvPkt.isData == 1)
            fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", stamp, "DATA", cpyRcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
        else
            fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", stamp, "ACK", cpyRcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
        fflush(fp);
        if(cpyRcvPkt.isData == 1)
            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", stamp, "DATA", cpyRcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
        else
            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", stamp, "ACK", cpyRcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
    }
    return 0;
}