#include "Q2_pktDef.h"

void die(char * error)
{
    perror(error);
    exit(-1);
}
char * timeStamp()
{
    char * stamp = (char *)malloc(sizeof(char)*50);
    int rc;
    time_t curr;
    struct tm * timePtr;
    struct timeval tv;
    
    curr = time(NULL);
    timePtr = localtime(&curr);
    gettimeofday(&tv, NULL);
    rc = strftime(stamp, 50, "%H:%M:%S", timePtr);

    char milliSec[8];
    sprintf(milliSec, ".%06ld", tv.tv_usec);
    strcat(stamp, milliSec);
    return stamp;
}
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
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
    srand(time(NULL));
    int skt, slen, slenR;
    struct sockaddr_in si_server, si_client, si_me, si_rcv;
    slen = sizeof(si_me);
    slenR = sizeof(si_rcv);
    memset((char *)&si_server, 0, slen);
    memset((char *)&si_me, 0, slen);
    memset((char *)&si_client, 0, slen);
    memset((char *)&si_rcv, 0, slenR);

    si_me.sin_addr.s_addr = inet_addr(IP);
    if(atoi(argv[1]) == 1)
        si_me.sin_port = htons(PORT_RELAY1);
    else if(atoi(argv[1]) == 2)
        si_me.sin_port = htons(PORT_RELAY2);
    si_me.sin_family = AF_INET;

    si_server.sin_addr.s_addr = inet_addr(IP);
    si_server.sin_port = htons(PORT_SERVER);
    si_server.sin_family = AF_INET;

    si_client.sin_addr.s_addr = inet_addr(IP);
    si_client.sin_port = htons(PORT_CLIENT);
    si_client.sin_family = AF_INET;

    if((skt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("socket()");
    
    if(bind(skt, (struct sockaddr*)&si_me, slen) < 0)
        die("bind()");
    
    PKT rcvPkt, cpyRcvPkt;
    int bytesSent, bytesRcvd, randNum;
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
        // bytesRcvd = recvfrom(skt, &rcvPkt, sizeof(rcvPkt), 0, NULL, NULL);
        if(bytesRcvd <= 0)
            die("recvfrom()");
        
        // if(bytesRcvd != sizeof(PKT))
        // {
        //     printf("BYE!! I am out\n");
        //     exit(0);
        // }

        randNum = rand()%3;
        msleep(randNum);

        randNum = rand()%100;
        if(testForDrop(randNum) && rcvPkt.isData == 1)
        {
            if(!strcmp(rcvPkt.dest, "CLIENT"))
                printf("PROBLEM\n");
            if(atoi(argv[1]) == 1)
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "D", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "D", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            fflush(fp);
            if(atoi(argv[1]) == 1)
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "D", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "D", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            continue;
        }
        // printf("%s\n", rcvPkt.payload);
        if(atoi(argv[1]) == 1)
        {
            if(rcvPkt.isData == 1)
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", timeStamp(), "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            fflush(fp);
            if(rcvPkt.isData == 1)
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            else
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY1", "R", timeStamp(), "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY1");
            strcpy(rcvPkt.src, "RELAY1\0");
        }
        else if(atoi(argv[1]) == 2)
        {
            if(rcvPkt.isData == 1)
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "R", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            else
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "RELAY2", "R", timeStamp(), "ACK", rcvPkt.seqNo, rcvPkt.src, "RELAY2");
            fflush(fp);
            strcpy(rcvPkt.src, "RELAY2\0");
        }
        makeCopy(&cpyRcvPkt, rcvPkt);
        //Route the pkt to appropriate next hop based on dest address in the received pkt
        if(!strcmp(rcvPkt.dest, "CLIENT\0"))
            bytesSent = sendto(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr*)&si_client, slen);
        else if(!strcmp(rcvPkt.dest, "SERVER\0"))
            bytesSent = sendto(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr*)&si_server, slen);
        
        if(bytesSent < 0)
            die("send()");

        if(cpyRcvPkt.isData == 1)
            fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", timeStamp(), "DATA", rcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
        else
            fprintf(fp, "%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", timeStamp(), "ACK", rcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
        fflush(fp);
        if(cpyRcvPkt.isData == 1)
            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", timeStamp(), "DATA", rcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
        else
            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
            cpyRcvPkt.src, "S", timeStamp(), "ACK", rcvPkt.seqNo, cpyRcvPkt.src, cpyRcvPkt.dest);
    }
    return 0;
}