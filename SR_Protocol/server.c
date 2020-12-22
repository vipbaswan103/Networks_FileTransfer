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

//To check whether the seq no lies within the bounds of the buffer
int checkCompatibility(int startOffset, int endOffset, int seqNo)
{
    if(seqNo >= startOffset && seqNo <= endOffset)
        return 1;
    return 0;
}

//Function to copy the string src to dest (needed if src isn't '\0' terminated)
void copyString(char *dest, char *src)
{
    for(int i=0; i<PACKET_SIZE; i++)
    {
        dest[i] = src[i];
    }
}


//Initialize the buffer
//startOffset = Least byte number that can be accepted
//endOffset = Highest byte number that can be accepted
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

//Insert the pkt in the buffer
void insertInBuffer(bufferEntry buffer[], int startOffset, PKT * pkt)
{
    int i = ((pkt->seqNo)-startOffset)/(PACKET_SIZE);
    buffer[i].isFilled = 1;
    buffer[i].sizeOfPayload = pkt->sizeOfPayload;
    buffer[i].isLastPkt = pkt->isLackPkt;
    copyString(buffer[i].payload, pkt->payload);
}

//Check whether buffer has some contiguous chunk of data
int checkContiguity(bufferEntry * buffer)
{
    int index = -1;
    
    //Search for the first non-filled entry in the buffer
    for(int i=0; i<BUFFERSIZE; i++)
    {
        if(buffer[i].isFilled == 0)
        {
            index = i;
            break;
        }
    }

    //We'll return 1 if complete buffer is empty
    if(index == -1)
        index = BUFFERSIZE;

    //Everything after this non-filled entry must be non-filled only (else there are holes in the buffer)
    for(int i=index; i<BUFFERSIZE; i++)
    {
        if(buffer[i].isFilled == 1)
            return 0;
    }

    return 1;
}

//Check whether the buffer has the last packet and has received everything before it 
//i.e. it must be contiguous and must have the last packet
int isLastAndContiguous(bufferEntry * buffer)
{
    //If buffer has holes, return 0
    if(checkContiguity(buffer) == 0)
        return 0;
    
    //Buffer is contiguous, check for last packet
    //NOTE: If last pkt exists in the buffer and buffer is contiguous,
    //then it is sure that file has been completely transferred
    for(int i=0; i<BUFFERSIZE; i++)
    {
        if(buffer[i].isFilled == 1 && buffer[i].isLastPkt == 1)
            return 1;
    }
    return 0;
}

//Empties the buffer by inserting the data into the file
int insertInFile(bufferEntry buffer[], int fd, int *startOffset, int *endOffset, int doWrite, int *quit)
{
    int isFilled = 0, size_written = 0;
    for(int i=0; i<BUFFERSIZE; i++)
    {
        isFilled += buffer[i].isFilled;
    }

    //Either buffer is completely filled or we have received the last packet
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
        {
            //Note: isLastAndContiguous return 0 if buffer is empty
            //If buffer is empty, return quit = 2 (i.e. conditional quit)
            //Now main() will check whether it has recieved last packet till now or not,
            //If yes, it'll exit, else continue to receive packets
            if(isFilled == 0)
                *quit = 2;
            return fd;
        }
        //Now, buffer has the last packet and everything has been transferred
        else if(doWrite == 2)   //File completely transferred, ask server to quit
            *quit = 1;
        
        //Write whatever is there in the buffer to the file
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

        //Only if doWrite = 0, we need to change offsets
        if(doWrite == 0)
        {
            (*startOffset) = (*endOffset) + 1;
            (*endOffset) = (*startOffset) + (BUFFERSIZE)*PACKET_SIZE - 1;
        }
    }
    return fd;
}

/*
void printIsFilled(bufferEntry * buffer)
{
    for(int i=0; i<BUFFERSIZE; i++)
    {
        printf("%d ", buffer[i].isFilled);
    }
    printf("\n");
}*/

int main()
{
    //Open the output.txt file if it exists,
    //Else, truncate it and open it
    int fd = open("output.txt", O_CREAT | O_WRONLY | O_TRUNC | __O_CLOEXEC, 0666);
    if(fd < 0)
        die("open(output.txt)\n");
    
    int skt, slen, slenR;
    struct sockaddr_in si_me, si_rcv;
    slen = sizeof(si_me);
    slenR = sizeof(si_rcv);
    memset((char *)&si_me, 0, slen);
    memset((char *)&si_rcv, 0, slenR);

    //Fill the server's address and bind socket to it
    si_me.sin_family = AF_INET;
    si_me.sin_addr.s_addr = inet_addr(IP);
    si_me.sin_port = htons(PORT_SERVER);

    if((skt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("socket()");
    
    if(bind(skt, (struct sockaddr *)&si_me, slen) < 0)
        die("bind()");
    
    PKT sendPkt, rcvPkt, cpySendPkt, cpyRcvPkt;
    int bytesSent, bytesRcvd, size_written, startOffset, endOffset, quit, gotLastPkt = 0;
    char * stamp = NULL;
    FILE * fp = fopen("log_server.txt", "w");
    if(fp == NULL)
        die("fopen()");

    //Create buffer and initialize its parameters
    bufferEntry buffer[BUFFERSIZE];
    initializeBuffer(buffer, &startOffset, &endOffset);

    while(1)
    {
        quit = 0;
        //Check whether we have received last pkt and everything before it
        fd = insertInFile(buffer, fd, &startOffset, &endOffset, 2, &quit);

        //quit = 1 means last pkt exists in the buffer and everything before it has also arrived
        //quit = 2 means buffer is empty. 
            //If last pkt has been received till now, quit
            //Else, keep receiving new packets
        if(quit == 1 || (gotLastPkt == 1 && quit == 2))
        {
            printf("\n\n\t****  All packets received successfully. Check output.txt!!  ****\n\n");
        }
        memset(&rcvPkt, 0, sizeof(rcvPkt));
        bytesRcvd = recvfrom(skt, &rcvPkt, sizeof(rcvPkt), 0, (struct sockaddr *)&si_rcv, &slenR);
        if(bytesRcvd <= 0)
            die("recvfrom()");

        //Get the rcv timestamp
        stamp = timeStamp();

        //Put event in the log
        fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "R", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
        printf("%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "R", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
        fflush(fp);

        //Check compatibility
        if((checkCompatibility(startOffset, endOffset, rcvPkt.seqNo)) == 1)
        {
            //Insert in the buffer, empty the buffer if its full or we have received last pkt and everything before it
            insertInBuffer(buffer, startOffset, &rcvPkt);
            quit = 0;
            fd = insertInFile(buffer, fd, &startOffset, &endOffset, rcvPkt.isLackPkt, &quit);
        }
        //Drop pkts which can't fit in the buffer
        else if(rcvPkt.seqNo > endOffset)
        {
            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
            "SERVER", "REJ", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
            "SERVER", "REJ", stamp, "DATA", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
            fflush(fp);
            continue;
        }

        //gotLastPkt will be set when we recieve the last pkt
        if(gotLastPkt == 0)
            gotLastPkt = rcvPkt.isLackPkt;
        
        //Prepare the ACK packet
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
        
        //Get the send timestamp and put the event in the log
        stamp = timeStamp();
        fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "S", stamp, "ACK", cpySendPkt.seqNo, cpySendPkt.src, cpyRcvPkt.src);
        printf("%10s %10s %20s %10s %10d %10s %10s\n", 
        "SERVER", "S", stamp, "ACK", cpySendPkt.seqNo, cpySendPkt.src, cpyRcvPkt.src);
        fflush(fp);
    }
}