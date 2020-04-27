#include "Q1_pktDef.h"

//To throw errors
void die(char * error)
{
    perror(error);
    exit(-1);
}

//This function return 1, if the pkt should be dropped. Else 0 is returned
//randNum: It is a random number between 0 to 99 (both inclusive)
int testForDrop(int randNum)
{
    if(randNum >= 0 && randNum < PDR)
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

int checkCompatibility(int startOffset, int endOffset, int seqNo)
{
    if(seqNo >= startOffset && seqNo <= endOffset)
        return 1;
    return 0;
}

void initializeBuffer(bufferEntry * buffer, int *startOffset, int *endOffset)
{
    for(int i=0; i<BUFSIZE; i++)
    {
        buffer[i].isFilled = 0;
    }
    *startOffset = 0;
    *endOffset = (BUFSIZE)*PACKET_SIZE - 1;
}

void insertInBuffer(bufferEntry * buffer, int startOffset, PKT * pkt)
{
    int i = ((pkt->seqNo)-startOffset)/(PACKET_SIZE);
    buffer[i].isFilled = 1;
    buffer[i].sizeOfPayload = pkt->sizeOfPayload;
    buffer[i].seqNo = pkt->seqNo;
    copyString(buffer[i].payload, pkt->payload);
}

int checkContiguity(bufferEntry * buffer)
{
    int index = -1;
    for(int i=0; i<BUFSIZE; i++)
    {
        if(buffer[i].isFilled == 0)
        {
            index = i;
            break;
        }
    }
    if(index == -1)
        index = BUFSIZE;
    for(int i=index; i<BUFSIZE; i++)
    {
        if(buffer[i].isFilled == 1)
            return 0;
    }
    return 1;
}
void insertInFile(bufferEntry * buffer, int fd, int *startOffset, int *endOffset, int doWrite)
{
    int isFilled = 0, size_written = 0;
    for(int i=0; i<BUFSIZE; i++)
    {
        isFilled += buffer[i].isFilled;
    }
    if((isFilled == BUFSIZE) || doWrite)
    {
        if(doWrite == 1 && !checkContiguity(buffer))
        {
            return;
        }
        printf("HERE\n");
        for(int i=0; i<BUFSIZE; i++)
        {
            if(buffer[i].isFilled == 1)
            {
                lseek(fd, buffer[i].seqNo, SEEK_SET);
                size_written = write(fd, buffer[i].payload, buffer[i].sizeOfPayload);
                if(size_written <= 0)
                    die("write()");
            }
            buffer[i].isFilled = 0;
        }
        if(doWrite == 0)
        {
            (*startOffset) = (*endOffset) + 1;
            (*endOffset) = (*startOffset) + (BUFSIZE)*PACKET_SIZE - 1;
        }
    }
    // return fd;
}
int main()
{
    //Set the seed so that rand() generated good number
    srand(time(NULL)); 

    //Open the output.txt file. If it doesn't exist, create it. If it exists, truncate the file to 0 bytes
    int fd = open("output.txt", O_CREAT | O_WRONLY | O_TRUNC | __O_CLOEXEC, 0666);
    if(fd < 0)
        die("open(output.txt)\n");
    
    //Declaring various resources needed later
    pid_t pid;
    int listenSkt, connSkt, slen, clen;
    struct sockaddr_in si_other, si_me;
    slen = sizeof(si_me);
    clen = sizeof(si_other);
    memset((char *)&si_me, 0, slen);
    memset((char *)&si_other, 0, slen);
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = inet_addr(IP);
    PKT sndPkt, rcvPkt, cpySndPkt, tmp;
    int offset, size_read, bytesSent, bytesRcvd, nready, size_written, randNum, dropOrNot;
    struct pollfd clients[3];
    
    //Open the skt that will be used for listening
    if((listenSkt = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        die("socket()\n");
    
    //Bind the socket to server's IP and PORT
    if(bind(listenSkt, (struct sockaddr*)&si_me, slen) == -1)
        die("bind()\n");
    
    //Mark the port as listerning for connections
    if(listen(listenSkt, MAXPENDING) == -1)
        die("listen()\n");
    
    bufferEntry buffer[BUFSIZE];
    int startOffset, endOffset;
    initializeBuffer(buffer, &startOffset, &endOffset);

    clients[0].fd = listenSkt;
    clients[0].events = POLLIN;
    printf("HELLO\n");
    int howMany = 1, howManyExitted = 0;
    while(1)
    {
        if(howManyExitted == 2)
        {
            insertInFile(buffer, fd, &startOffset, &endOffset, 1);
            break;
        }
        nready = poll(clients, howMany, -1);
        if(nready > 0)
        {
            //There is something to accept
            if(clients[0].revents & POLLIN)
            {
                connSkt = accept(listenSkt, (struct sockaddr*)&si_other, &clen);
                if(connSkt < 0)
                    die("accept()");
                clients[howMany].fd = connSkt;
                clients[howMany].events = POLLIN;
                howMany++;
                continue;
            }
            if((clients[1].revents & POLLIN) || (clients[2].revents & POLLIN))
            {
                if(clients[1].revents & POLLIN)
                    connSkt = clients[1].fd; 
                else
                    connSkt = clients[2].fd; 

                bytesRcvd = recv(connSkt, &rcvPkt, sizeof(rcvPkt), MSG_WAITALL);
                //If bytesRcvd is 0, then connection has been gracefully closed by the client
                //Else, some error occured in recv()
                if(bytesRcvd <= 0)
                {
                    if(bytesRcvd == 0)
                    {
                        printf("Here\n");
                        // insertInFile(buffer, fd, &startOffset, &endOffset, 1);
                        //make fd < 0
                        if(clients[1].revents & POLLIN)
                            clients[1].fd = -1;
                        else
                            clients[2].fd = -1;
                        howManyExitted++;
                        continue;
                    }
                    else
                        die("recv()\n");
                }

                //Test whether to drop this pkt or not
                randNum = rand()%100;
                dropOrNot = testForDrop(randNum);

                if(dropOrNot == 1)
                {
                    printf("PKT DROPPED: Seq. No %d dropped from channel %d\n", rcvPkt.seqNo, rcvPkt.channelID);
                    continue;
                }

                //Data pkt rcvd, push the msg to o/p log
                printf("RCVD PKT: Seq. No %d of size %d bytes from channel %d\n",
                rcvPkt.seqNo, rcvPkt.sizeOfPayload, rcvPkt.channelID);

                offset = rcvPkt.seqNo;

                if(checkCompatibility(startOffset, endOffset, rcvPkt.seqNo) == 1)
                {
                    insertInBuffer(buffer, startOffset, &rcvPkt);
                    insertInFile(buffer, fd, &startOffset, &endOffset, rcvPkt.isLastPkt);
                    // lseek(fd, offset, SEEK_SET);
                    // write(fd, rcvPkt.payload, rcvPkt.sizeOfPayload);
                }
                else if(rcvPkt.seqNo > endOffset)
                {
                    printf("PKT REJECTED: Seq. No %d of size %d bytes from channel %d\n",
                    rcvPkt.seqNo, rcvPkt.sizeOfPayload, rcvPkt.channelID);
                    continue;
                }

                //Prepare the ACK pkt
                sndPkt.channelID = rcvPkt.channelID;
                sndPkt.isData = 0;
                sndPkt.isLastPkt = rcvPkt.isLastPkt;
                memset(sndPkt.payload, '\0', sizeof(sndPkt.payload));
                sndPkt.seqNo = rcvPkt.seqNo;
                sndPkt.sizeOfPayload = 0;
                
                //Send the ACK to the client
                bytesSent = send(connSkt, &sndPkt, sizeof(sndPkt), 0);
                if(bytesSent != sizeof(sndPkt))
                    die("send()\n");
                printf("SENT ACK: For PKT with Seq. No. %d from channel %d\n",
                sndPkt.seqNo, sndPkt.channelID);
            }
        }
    }
    // insertInFile(buffer, fd, &startOffset, &endOffset, 1);
    if(close(listenSkt) < 0)
    {
        die("close(listenSkt)\n");
    }
}