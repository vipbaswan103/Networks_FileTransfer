#include "Q2_pktDef.h"

void die(char * error)
{
    perror(error);
    exit(-1);
}

void copyString(char *dest, char *src)
{
    for(int i=0; i<PACKET_SIZE; i++)
    {
        dest[i] = src[i];
    }
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
    copyString(cpy->payload, sndPkt.payload);
    cpy->seqNo = sndPkt.seqNo;
    cpy->sizeOfPayload = sndPkt.sizeOfPayload;
}

int main()
{
    int fd = open("input.txt", O_RDONLY);
    if(fd < 0)
        die("open(input.txt)");
    
    int skt, slen, slenR;
    struct sockaddr_in si_other[2], si_rcv, si_me;
    slen = sizeof(si_other[0]);
    slenR = sizeof(si_rcv);
    memset((char *)&si_rcv, 0, sizeof(char)*slenR);
    memset((char *)&si_me, 0, sizeof(char)*slen);
    si_me.sin_family = AF_INET;
    si_me.sin_addr.s_addr = inet_addr(IP);
    si_me.sin_port = htons(PORT_CLIENT);
    for(int i=0; i<2; i++)
    {
        memset((char *)&si_other[i], 0, sizeof(char)*slen);
        si_other[i].sin_family = AF_INET;
        if(i==0)
            si_other[i].sin_port = htons(PORT_RELAY1);
        else
            si_other[i].sin_port = htons(PORT_RELAY2);
        si_other[i].sin_addr.s_addr = inet_addr(IP);
    }
    struct pollfd clients[1];

    if((skt = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("socket()");
    if(bind(skt, (struct sockaddr*)&si_me, slen) < 0)
        die("bind()");
    PKT sndPkt[WINDOW_SIZE], rcvPkt, cpySndPkt[WINDOW_SIZE], tmp;
    int offset, size_read, bytesSent, bytesRcvd, nready, isACKed[WINDOW_SIZE], fileOffsetStart, fileOffsetEnd,
    numAcked, sentPkts, file_size;

    file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    FILE * fp = fopen("log_client.txt", "w");
    if(fp == NULL)
        die("fopen()\n");
    clients[0].fd = skt;
    clients[0].events = POLLIN;
    while(1)
    {
        //Send all the packets in the current window
        sentPkts = 0;
        for(int i=0; i<WINDOW_SIZE; i++)
        {
            offset = lseek(fd, 0, SEEK_CUR);
            strcpy(sndPkt[i].src, "CLIENT\0");
            strcpy(sndPkt[i].dest, "SERVER\0");
            sndPkt[i].isData = 1;
            sndPkt[i].seqNo = offset;
            sndPkt[i].sizeOfPayload = PACKET_SIZE;
            sndPkt[i].isLackPkt = 0;
            memset(sndPkt[i].payload, '\0', sizeof(char)*PACKET_SIZE);
            size_read = read(fd, sndPkt[i].payload, PACKET_SIZE);
            if(size_read == 0)
            {
                if(i == 0)
                {
                    // printf("Here\n");
                    exit(0);
                }
                break;
            }
            offset = lseek(fd, 0, SEEK_CUR);
            if(offset == file_size)
                sndPkt[i].isLackPkt = 1;
            
            if(size_read != PACKET_SIZE)
            {
                if(size_read < 0)
                    die("read()");
                else if(size_read < PACKET_SIZE)
                {
                    sndPkt[i].isLackPkt = 1;
                    sndPkt[i].sizeOfPayload = size_read;
                }
            }
            
            makeCopy(&cpySndPkt[i], sndPkt[i]);
            bytesSent = sendto(skt, &sndPkt[i], sizeof(sndPkt[i]), 0, 
            (struct sockaddr *)&si_other[i%2], slen);
            if(bytesSent != sizeof(sndPkt[i]))
                die("sendto()");
            
            if(i%2 == 0)
            {
                fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                "CLIENT", "S", timeStamp(), "DATA",cpySndPkt[i].seqNo, "CLIENT", "RELAY1");
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "CLIENT", "S", timeStamp(), "DATA",cpySndPkt[i].seqNo, "CLIENT", "RELAY1");
            }
            else
            {
                fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                "CLIENT", "S", timeStamp(), "DATA", cpySndPkt[i].seqNo, "CLIENT", "RELAY2");
                printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                "CLIENT", "S", timeStamp(), "DATA", cpySndPkt[i].seqNo, "CLIENT", "RELAY2");
            }
            
            sentPkts++;
            isACKed[i] = 0;
        }

        // printf("%d\n", sentPkts);
        fileOffsetStart = sndPkt[0].seqNo;
        fileOffsetEnd = sndPkt[sentPkts-1].seqNo;

        //Now wait for ACKs for all the packets
        printf("WINDOW SIZE:\t%d\t%d\n",fileOffsetStart, fileOffsetEnd);
        while(1)
        {
            nready = poll(clients, 1, TIMEOUT);

            //If TIMEOUT occurs, send the unacked pkts back to server
            if(nready == 0)
            {
                numAcked = 0;
                for(int i=0; i<sentPkts; i++)
                {
                    makeCopy(&tmp, cpySndPkt[i]);
                    if(isACKed[i] == 0)
                    {
                        if(i%2 == 0)
                        {
                            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "TO", timeStamp(), "DATA", cpySndPkt[i].seqNo, "CLIENT", "RELAY1");
                            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "TO", timeStamp(), "DATA", cpySndPkt[i].seqNo, "CLIENT", "RELAY1");
                        }
                        else
                        {
                            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "TO", timeStamp(), "DATA", cpySndPkt[i].seqNo, "CLIENT", "RELAY2");
                            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "TO", timeStamp(), "DATA", cpySndPkt[i].seqNo, "CLIENT", "RELAY2");
                        }

                        bytesSent = sendto(skt, &tmp, sizeof(tmp), 0, 
                        (struct sockaddr *)&si_other[i%2], slen);
                        if(bytesSent != sizeof(cpySndPkt[i]))
                            die("sendto()");
                        if(i%2 == 0)
                        {
                            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY1");
                            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY1");
                        }
                        else
                        {
                            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY2");
                            printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                            "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY2");
                        }
                    }
                    else
                        numAcked++;
                }
                if(numAcked == sentPkts)
                    break;
                continue;
            }
            //Something is available to be read
            else if(nready > 0)
            {
                // Now, we will read all that is available in the rcv buffer till now
                while(1)
                {
                    //Poll with timeout 0, this means: Return >0 if something is available else 0
                    nready = poll(clients, 1, 0);

                    //Something is available
                    if(nready > 0)
                    {
                        memset(rcvPkt.payload, '\0',sizeof(char)*PACKET_SIZE);
                        bytesRcvd = recvfrom(skt, &rcvPkt, sizeof(rcvPkt), 0,
                        (struct sockaddr *)&si_rcv, &slenR);
                        if(bytesRcvd <= 0)
                            die("recvfrom()");
                        //Check that you have received the correct ACK, if not, simply ignore the ACK
                        if(fileOffsetStart <= rcvPkt.seqNo && rcvPkt.seqNo <= fileOffsetEnd)
                        {
                            // if(rcvPkt.isLackPkt == 0)
                            isACKed[(rcvPkt.seqNo - fileOffsetStart)/PACKET_SIZE] = 1;
                            // else
                            //     isACKed[sentPkts-1] = 1;
                            fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n",
                            "CLIENT", "R", timeStamp(), "ACK", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
                            printf("%10s %10s %20s %10s %10d %10s %10s\n",
                            "CLIENT", "R", timeStamp(), "ACK", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
                        }
                        else
                        {
                            printf("%10s %10s %20s %10s %10d %10s %10s\n",
                            "CLIENT", "OUTORDER", timeStamp(), "ACK", rcvPkt.seqNo, rcvPkt.src, rcvPkt.dest);
                        }
                        
                    }
                    //Nothing else is available, now check whether everything in the window is ACKed yet or not
                    //If not, again continue in the outer loop
                    else if(nready == 0)
                    {
                        numAcked = 0;
                        for(int i=0; i<sentPkts; i++)
                        {
                            numAcked += isACKed[i];
                            makeCopy(&tmp, cpySndPkt[i]);
                            if(isACKed[i] == 0)
                            {
                                // printf("YES\n");
                                bytesSent = sendto(skt, &tmp, sizeof(tmp), 0, 
                                (struct sockaddr *)&si_other[i%2], slen);
                                if(bytesSent != sizeof(cpySndPkt[i]))
                                    die("sendto()");
                                if(i%2 == 0)
                                {
                                    fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                                    "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY1");
                                    printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                                    "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY1");
                                }
                                else
                                {
                                    fprintf(fp,"%10s %10s %20s %10s %10d %10s %10s\n", 
                                    "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY2");
                                    printf("%10s %10s %20s %10s %10d %10s %10s\n", 
                                    "CLIENT", "RE", timeStamp(), "DATA", cpySndPkt[i].seqNo,"CLIENT", "RELAY2");
                                }
                            }
                        }
                        break;
                    }
                }
                // printf("%d %d\n", numAcked, sentPkts);
                if(numAcked == sentPkts)
                    break;
                continue;
            }
        }
        //Now, whole window has been ACKed
        //Continue transferring the file
    }
    if(close(skt)<0)
        die("close()");
    return 0;
}