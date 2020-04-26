#include "Q2_pktDef.h"

void die(char * error)
{
    perror(errno);
    exit(-1);
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

int main()
{
    int fd = open("input.txt", O_RDONLY);
    if(fd < 0)
        die("open(input.txt)");
    
    int skt[2], slen, slenR;
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
    struct pollfd clients[2];

    if((skt[0] = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("skt1 socket()");
    if((skt[1] = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        die("skt2 socket()");
    
    PKT sndPkt[WINDOW_SIZE], rcvPkt, cpySndPkt[WINDOW_SIZE], tmp[WINDOW_SIZE];
    int offset, size_read, bytesSent, bytesRcvd, nready, isACKed[WINDOW_SIZE], fileOffsetStart, fileOffsetEnd,
    numAcked, sentPkts;

    clients[0].fd = skt[0];
    clients[0].events = POLLIN;
    clients[1].fd = skt[1];
    clients[1].events = POLLIN;
    while(1)
    {
        //Send all the packets in the current window
        for(int i=0; i<WINDOW_SIZE; i++)
        {
            offset = lseek(fd, 0, SEEK_CUR);
            strcpy(sndPkt[i].src, "CLIENT\0");
            strcpy(sndPkt[i].dest, "RELAY1\0");
            sndPkt[i].isData = 1;
            sndPkt[i].seqNo = offset;
            sndPkt[i].sizeOfPayload = PACKET_SIZE;
            sndPkt[i].isLackPkt = 0;
            memset(sndPkt[i].payload, '\0', sizeof(char)*PACKET_SIZE);
            size_read = read(fd, sndPkt[i].payload, PACKET_SIZE);
            if(size_read == 0)
            {
                if(i == 0)
                    exit(0);
                break;
            }
            if(size_read != PACKET_SIZE)
            {
                if(size_read < 0)
                    die("read()");
                else
                {
                    sndPkt[i].isLackPkt = 1;
                    sndPkt[i].sizeOfPayload = size_read;
                }
            }
            makeCopy(&cpySndPkt[i], sndPkt[i]);
            bytesSent = sendto(skt[i%2], &sndPkt[i], sizeof(sndPkt[i]), 0, 
            (struct sockaddr *)&si_other[i%2], slen);
            if(bytesSent != sizeof(sndPkt[i]))
                die("sendto()");
            sentPkts++;
            isACKed[i] = 0;
        }
        fileOffsetStart = sndPkt[0].seqNo;
        fileOffsetEnd = (sndPkt[sentPkts-1].seqNo) + sndPkt[sentPkts-1].sizeOfPayload;

        //Now wait for ACKs for all the packets
        while(1)
        {
            nready = poll(clients, 2, TIMEOUT);

            //If TIMEOUT occurs, send the unacked pkts back to server
            if(nready == 0)
            {
                for(int i=0; i<sentPkts; i++)
                {
                    if(isACKed[i] == 0)
                    {
                        makeCopy(&tmp[i], cpySndPkt[i]);
                        bytesSent = sendto(skt[i%2], &cpySndPkt[i], sizeof(cpySndPkt[i]), 0, 
                        (struct sockaddr *)&si_other[i%2], slen);
                        makeCopy(&cpySndPkt[i], tmp[i]);
                        if(bytesSent != sizeof(sndPkt[i]))
                            die("sendto()");
                    }
                }
                continue;
            }
            //Something is available to be read
            else if(nready > 0)
            {

                //Now, we will read all that is available in the rcv buffer till now
                while(1)
                {
                    //Poll with timeout 0, this means: Return >0 if something is available else 0
                    nready = poll(clients, 2, 0);

                    //Something is available
                    if(nready > 0)
                    {
                        //Check both the skts (since we can get ACK from any channel)
                        for(int i=0; i<2; i++)
                        {
                            if(clients[i].revents & POLLIN)
                            {
                                memset(rcvPkt.payload, '\0',sizeof(char)*PACKET_SIZE);
                                bytesRcvd = recvfrom(skt[i], &rcvPkt, sizeof(rcvPkt), 0,
                                (struct sockaddr *)&si_other[i%2], &slenR);

                                //Check that you have received the correct ACK, if not, simply ignore the ACK
                                if(fileOffsetStart <= rcvPkt.seqNo && rcvPkt.seqNo <= fileOffsetEnd)
                                {
                                    isACKed[(rcvPkt.seqNo - fileOffsetStart)/PACKET_SIZE] = 1;
                                }
                                if(bytesRcvd < 0)
                                    die("recvfrom()");
                            }
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
                        }
                        break;
                    }
                }
                if(numAcked == sentPkts)
                    break;
                continue;
            }
        }

        //Now, whole window has been ACKed
        //Continue transferring the file
    }
    if(close(skt[0])<0)
        die("skt0: close()");
    if(close(skt[1])<0)
        die("skt1: close()");
    return 0;
}