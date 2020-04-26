#include "Q1_pktDef.h"

void die(char * error)
{
    perror(error);
    exit(-1);
}

int main()
{
    int skt[2], slen;
    struct sockaddr_in si_other;
    slen = sizeof(si_other);
    PKT sndPkt, rcvPkt, cpySndPkt;
    struct pollfd clients[1];

    FILE * fp = fopen("input.txt", "r");
    if(fp == NULL)
        die("Error in opening the file \"input.txt\"\n");

    if((skt[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        die("Error while creating the socket for channel 0\n");
    
    if((skt[1] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        die("Error while creating the socket for channel 1\n");

    /*Filling the structure of si_other*/
    memset((char *)&si_other, 0, slen);
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    si_other.sin_addr.s_addr = inet_addr(IP);
    
    /*Connect each socket to server*/
    if(connect(skt[0], (struct sockaddr*) &si_other, slen) < 0)
        die("Error in connecting socket 0 to the server\n");
    if(connect(skt[1], (struct sockaddr*) &si_other, slen) < 0)
        die("Error in connecting socket 1 to the server\n");

    /*Preparing clients array for socket*/
    clients[0].fd = skt[0];
    clients[0].events = POLLIN;
    clients[1].fd = skt[1];
    clients[1].events = POLLIN;

    /*Set seed to generate random numbers (to select to which channel data is sent)*/
    srand(time(0));

    /*The state information of transmission*/
    int size_read = -1, nextSeqNum = _SC_INT_MIN, bytesSent, bytesRcvd, whichChannel, nready;
    int isAcked[2]; //Stores thte status of ack for each channel
                    //If isAcked[0] = 1, it means for data pkt sent through channel 0, no ack has been recieved
    isAcked[0] = 1;
    isAcked[1] = 1;

    while(1)
    {
        // if(isAcked[0] == 1 || isAcked[1] == 1)
        // {
        sndPkt.isData = 1;
        sndPkt.seqNo = nextSeqNum;
        size_read = fread(sndPkt.payload, PACKET_SIZE, 1, fp);
        sndPkt.sizeOfPayload = PACKET_SIZE;

        if(isAcked[0] == 1 && isAcked[1] == 1)
            sndPkt.channelID = rand()%2;
        else if(isAcked[0] == 1)
            sndPkt.channelID = 0;
        else
            sndPkt.channelID = 1;
        sndPkt.isLastPkt = 0;
        
        whichChannel = sndPkt.channelID;
        if(size_read < PACKET_SIZE)
        {
            if(feof(fp))
            {
                sndPkt.isLastPkt = 1;
                sndPkt.sizeOfPayload = size_read;
            }
            if(ferror(fp))
                die("Error while reading the file \"input.txt\"\n");    
        }
        cpySndPkt = sndPkt;
        if(whichChannel == 0)
            bytesSent = send(skt[0], &sndPkt, sizeof(sndPkt), 0);
        else
            bytesSent = send(skt[1], &sndPkt, sizeof(sndPkt), 0);
        
        if(bytesSent != sizeof(sndPkt))
            die("Error while sending the data to server\n");

        // if(whichChannel == 0)
        // {
        //     clients[0].fd = skt[0];
        //     clients[0].events = POLLIN;
        //     nready = poll(clients, 1, TIMEOUT);
        // }
        // else
        // {
        //     clients[0].fd = skt[1];
        //     clients[0].events = POLLIN;
        //     nready = poll(clients, 1, TIMEOUT);
        // }

        /*Poll to check which one of the two sockets has ACK available so that it can be read*/
        nready = poll(clients, 2, TIMEOUT);

        /*TIMEOUT: This means that we need to retransmit the pkt*/
        while(nready < 0)
        {
            if(whichChannel == 0)
                bytesSent = send(skt[0], &cpySndPkt, sizeof(cpySndPkt), 0);
            else
                bytesSent = send(skt[1], &cpySndPkt, sizeof(cpySndPkt), 0);
            if(bytesSent != sizeof(cpySndPkt))
                die("Error while sending the data to server\n");
            nready = poll(clients, 2, TIMEOUT);
        }
        for(int i=0; i<2; i++)
        {
            if(clients[i].revents & POLLIN)
            {
                bytesRcvd = recv(skt[i], &rcvPkt, sizeof(rcvPkt), 0);

                if(bytesRcvd < 0)
                    die("Error while reading ACK from the server\n");

                if(rcvPkt.isLastPkt == 1)
                {
                    return 0;   //File has been uploaded on server
                }
                bytesRcvd = recv(skt[i], &rcvPkt, sizeof(rcvPkt), 0);
                isAcked[i] = 1;
                nextSeqNum = max(rcvPkt.seqNo + rcvPkt.sizeOfPayload, nextSeqNum);
            }
        }
        // }
        // else
        // {
        //     nready = poll(clients, 2, TIMEOUT);

        //     for(int i=0; i<2; i++)
        //     {
        //         if(clients[i].revents & POLLIN)
        //         {
        //             if(rcvPkt.isLastPkt == 1)
        //             {
        //                 return 0;   //File has been uploaded on server
        //             }
        //             bytesRcvd = recv(skt[i], &rcvPkt, sizeof(rcvPkt), 0);
        //             isAcked[i] = 1;
        //             nextSeqNum = max(rcvPkt.seqNo + rcvPkt.sizeOfPayload, nextSeqNum);
        //         }
        //     }
        // }
    }
}

// }
    // else    //Parent process, handles the channel 0
    // {
    //     if((skt = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    //         die("Error while creating the socket for channel 0\n");
    
    //     if(connect(skt, (struct sockaddr*) &si_other, slen) < 0)
    //         die("Error in connecting socket 0 to the server\n");

    //     clients[0].fd = skt;
    //     clients[0].events = POLLIN;
    //     while(1)
    //     {
    //         offset = lseek(fd, 0, SEEK_CUR);
    //         if(offset == lseek(fd, 0, SEEK_END))
    //         {
    //             // die("File successfully uploaded on the server\n");
    //             exit(0);
    //         }
    //         lseek(fd, offset, SEEK_SET);
    //         sndPkt.isData = 1;
    //         size_read = read(fd, sndPkt.payload, PACKET_SIZE);
    //         sndPkt.seqNo = offset;
    //         sndPkt.sizeOfPayload = PACKET_SIZE;
    //         sndPkt.channelID = 0;
    //         sndPkt.isLastPkt = 0;
    //         if(size_read != PACKET_SIZE)
    //         {
    //             if(size_read == 0)
    //             {
    //                 // int tmp = lseek(fd, 0, SEEK_END);
    //                 // sndPkt.sizeOfPayload = (tmp - offset + 1);
    //                 sndPkt.isLastPkt = 1;
    //             }
    //             else
    //             {
    //                 die("Error while reading from the file (channel 0)\n");
    //             }
    //         }
    //         cpySndPkt = sndPkt;
    //         bytesSent = send(skt, &sndPkt, sizeof(sndPkt), 0);
    //         if(bytesSent != sizeof(sndPkt))
    //             die("Error while sending the data to server (from channel 0)\n");
            
    //         printf("SENT PKT: Seq. No %d of size %d bytes from channel 0\n", 
    //         cpySndPkt.seqNo, cpySndPkt.sizeOfPayload);
    //         printf("Payload: %s\n", cpySndPkt.payload);

    //         nready = poll(clients, 1, TIMEOUT);
    //         while(nready == -1)
    //         {
    //             tmp = cpySndPkt;
    //             bytesSent = send(skt, &cpySndPkt, sizeof(cpySndPkt), 0);
    //             if(bytesSent != sizeof(cpySndPkt))
    //                 die("Error while sending the data to server (from channel 0)\n");
    //             printf("SENT PKT: Seq. No %d of size %d bytes from channel 0\n", 
    //             cpySndPkt.seqNo, cpySndPkt.sizeOfPayload);
    //             printf("Payload: %s\n", cpySndPkt.payload);
    //             nready = poll(clients, 1, TIMEOUT);
    //             cpySndPkt = tmp;
    //         }
    //         bytesRcvd = recv(skt, &rcvPkt, sizeof(rcvPkt), 0);
    //         if(bytesRcvd < 0)
    //             die("Error while receiving the data from server (into channel 0)\n");
    //         printf("RCVD ACK: For pkt with Seq. No %d from channel 0\n", 
    //         rcvPkt.seqNo);
    //     }
    // }