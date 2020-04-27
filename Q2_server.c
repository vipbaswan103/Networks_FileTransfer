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

int checkCompatibility(int startOffset, int endOffset, int seqNo)
{
    if(seqNo >= startOffset && seqNo <= endOffset)
        return 1;
    return 0;
}

void copyString(char *dest, char *src)
{
    for(int i=0; i<PACKET_SIZE; i++)
    {
        dest[i] = src[i];
    }
}

void initializeBuffer(bufferEntry buffer[], int *startOffset, int *endOffset)
{
    for(int i=0; i<BUFFERSIZE; i++)
    {
        buffer[i].isFilled = 0;
        buffer[i].sizeOfPayload = 0;
        buffer[i].isLastPkt = 0;
        memset(buffer[i].payload, '\0', sizeof(char)*PACKET_SIZE);
    }
    *startOffset = 0;
    *endOffset = (BUFFERSIZE)*PACKET_SIZE - 1;
}

void insertInBuffer(bufferEntry buffer[], int startOffset, PKT * pkt)
{
    int i = ((pkt->seqNo)-startOffset)/(PACKET_SIZE);
    buffer[i].isFilled = 1;
    buffer[i].sizeOfPayload = pkt->sizeOfPayload;
    buffer[i].isLastPkt = pkt->isLackPkt;
    copyString(buffer[i].payload, pkt->payload);
}
int checkContiguity(bufferEntry * buffer)
{
    int index = -1;
    for(int i=0; i<BUFFERSIZE; i++)
    {
        if(buffer[i].isFilled == 0)
        {
            index = i;
            break;
        }
    }
    if(index == -1)
        index = BUFFERSIZE;
    for(int i=index; i<BUFFERSIZE; i++)
    {
        if(buffer[i].isFilled == 1)
            return 0;
    }
    return 1;
}
int isLastAndContiguous(bufferEntry * buffer)
{
    if(checkContiguity(buffer) == 0)
        return 0;
    
    //NOTE: If last pkt exists in the buffer and buffer is contiguous,
    //then it is sure that file has been completely transferred
    for(int i=0; i<BUFFERSIZE; i++)
    {
        if(buffer[i].isFilled == 1 && buffer[i].isLastPkt == 1)
            return 1;
    }
    return 0;
}
int insertInFile(bufferEntry buffer[], int fd, int *startOffset, int *endOffset, int doWrite, int *quit)
{
    int isFilled = 0, size_written = 0;
    for(int i=0; i<BUFFERSIZE; i++)
    {
        isFilled += buffer[i].isFilled;
    }
    if((isFilled == BUFFERSIZE) || doWrite)
    {
        //If doWrite == 1, it means last packet has arrived just now
        //If lastpkt just arrived now, check whether everything else has already arrived or not
        //If yes, put buffer to file and ask server to quit
        //Else, ignore and return
        if(doWrite == 1 && !checkContiguity(buffer))
            return fd;
        else if(doWrite == 1)
            *quit = 1;
        
        //If doWrite == 2, it means we don't know whether lastpkt has arrived or not
        //If it has arrived and everything else also has arrived, then transfer buffer to the file
        //and ask server to quit. Else, ignore and just return
        if(doWrite == 2 && !isLastAndContiguous(buffer))
            return fd;
        else if(doWrite == 2)   //File completely transferred, ask server to quit
            *quit = 1;

        for(int i=0; i<BUFFERSIZE; i++)
        {
            if(buffer[i].isFilled == 1)
            {
                size_written = write(fd, buffer[i].payload, buffer[i].sizeOfPayload);
                if(size_written <= 0)
                    die("write()");
            }
            buffer[i].isFilled = 0;
        }

        if(doWrite == 0)
        {
            (*startOffset) = (*endOffset) + 1;
            (*endOffset) = (*startOffset) + (BUFFERSIZE)*PACKET_SIZE - 1;
        }
    }
    return fd;
}

void printIsFilled(bufferEntry * buffer)
{
    for(int i=0; i<BUFFERSIZE; i++)
    {
        printf("%d ", buffer[i].isFilled);
    }
    printf("\n");
}
int main()
{
    int fd = open("output.txt", O_CREAT | O_WRONLY | O_TRUNC | __O_CLOEXEC, 0666);
    if(fd < 0)
        die("open(output.txt)\n");
    
    int skt, slen, slenR;
    struct sockaddr_in si_me, si_rcv;
    slen = sizeof(si_me);
    slenR = sizeof(si_rcv);
    memset((char *)&si_me, 0, slen);
    memset((char *)&si_rcv, 0, slenR);

    si_me.sin_family = AF_INET;
    si_me.sin_addr.s_addr = inet_addr(IP);
    si_me.sin_port = htons(PORT_SERVER);

    if((skt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("socket()");
    
    if(bind(skt, (struct sockaddr *)&si_me, slen) < 0)
        die("bind()");
    
    PKT sendPkt, rcvPkt, cpySendPkt, cpyRcvPkt;
    int bytesSent, bytesRcvd, size_written, startOffset, endOffset, quit;
    FILE * fp = fopen("log_server.txt", "w");
    if(fp == NULL)
        die("fopen()");
    bufferEntry buffer[BUFFERSIZE];
    initializeBuffer(buffer, &startOffset, &endOffset);

    while(1)
    {
        fd = insertInFile(buffer, fd, &startOffset, &endOffset, 2, &quit);
        if(quit == 1)
        {
            printf("FILE transferred successfully!! Exiting ...\n");
            exit(0);
        }
        bytesRcvd = recvfrom(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr *)&si_rcv, &slenR);
        if(bytesRcvd <= 0)
            die("recvfrom()");

        fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "R", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
        printf("%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "R", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
        
        if((checkCompatibility(startOffset, endOffset, rcvPkt.seqNo)) == 1)
        {
            insertInBuffer(buffer, startOffset, &rcvPkt);
            fd = insertInFile(buffer, fd, &startOffset, &endOffset, rcvPkt.isLackPkt, &quit);
        }
        else if(rcvPkt.seqNo > endOffset)
        {
            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
            "SERVER", "REJ", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
            "SERVER", "REJ", timeStamp(), "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
            continue;
        }
        strcpy(sendPkt.dest, "CLIENT\0");
        strcpy(sendPkt.src, "SERVER\0");
        sendPkt.isData = 0;
        sendPkt.isLackPkt = rcvPkt.isLackPkt;
        sendPkt.seqNo = rcvPkt.seqNo;
        sendPkt.sizeOfPayload = 0;
        memset(sendPkt.payload, '\0', sizeof(char)*PACKET_SIZE);

        makeCopy(&cpySendPkt, sendPkt);
        makeCopy(&cpyRcvPkt, rcvPkt);
        bytesSent = sendto(skt, &sendPkt, sizeof(sendPkt), 0, (struct sockaddr *)&si_rcv, slenR);
        if(bytesSent < 0)
            die("sendto()");
        
        fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "S", timeStamp(), "ACK", cpySendPkt.seqNo, cpySendPkt.src, cpyRcvPkt.src);
        printf("%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "S", timeStamp(), "ACK", cpySendPkt.seqNo, cpySendPkt.src, cpyRcvPkt.src);
    }
}