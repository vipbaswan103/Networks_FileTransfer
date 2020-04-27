#include "Q1_pktDef.h"

//To throw errors
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

//For making a deep copy of the pkt (may be needed later for retransmissions)
void makeCopy(PKT * cpy, PKT sndPkt)
{
    cpy->channelID = sndPkt.channelID;
    cpy->isData = sndPkt.isData;
    cpy->isLastPkt = sndPkt.isLastPkt;
    copyString(cpy->payload, sndPkt.payload);
    cpy->seqNo = sndPkt.seqNo;
    cpy->sizeOfPayload = sndPkt.sizeOfPayload;
}

int main()
{
    //Open the file "input.txt" for reading
    int fd = open("input.txt", O_RDONLY);
    if(fd < 0)
        die("open(input.txt)");

    //Declaring various resources needed later
    pid_t pid;
    int skt, slen;
    struct sockaddr_in si_other;
    slen = sizeof(si_other);
    memset((char *)&si_other, 0, slen);
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
    si_other.sin_addr.s_addr = inet_addr(IP);
    struct pollfd clients[1];
    PKT sndPkt, rcvPkt, cpySndPkt, tmp;
    int size_read, bytesSent, bytesRcvd, nready, file_size, numTrans, offset = 0;

    //Store the size of the file, will be used to know when to stop sending
    file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    sem_t *sem;
    //Now, we initialize our shared semaphore with initial value 1
    sem = sem_open ("semp", O_CREAT | O_EXCL, 0644, 1); 
    if(sem == NULL)
    {
        //Semaphore with same name "semp" already exists
        if(errno == EEXIST)
        {
            //Unlink the semaphore
            sem_unlink("semp");
            sem_close(sem);

            //Now, create a fresh semaphore
            sem = sem_open ("semp", O_CREAT | O_EXCL, 0644, 1); 
        }
        else
            die("sem_open()");
    }

    //Now, we will fork() to create 2 clients
    pid = fork();
    if(pid < 0)
    {
        sem_unlink("semp");
        sem_close(sem);
        die("fork()");
    }


        /*          
        *          NOTE: From this point onwards,
        *          1) All code executes for both child and the parent
        *          2) We'll check pid for deciding what channel number should we give for a pkt to be sent
        *          3) For our case, parent is connected to channel no 1
        *              and child is connected to channe no 0
        */


    //Create the socket
    if((skt = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        sem_unlink("semp");
        sem_close(sem);
        die("socket()");
    }

    //Connect the socket to the server
    if(connect(skt, (struct sockaddr*) &si_other, slen) < 0)
    {
        sem_unlink("semp");
        sem_close(sem);
        die("connect()");
    }
    
    //Now initialize the fd_set (it will be used for polling later)
    clients[0].fd = skt;
    clients[0].events = POLLIN;

    //Do this indefinitely (we'll break only if complete file is transferred)
    while(1)
    {   
        memset((char *)sndPkt.payload, '\0', sizeof(char)*PACKET_SIZE);

        /*NOTE: lseek() is used to know from which byte do we need to send data
        *       This byte_number will be used as sequence number
        *       Then, we'll read from this byte_number
        *       All this has to be done automatically 
        *       (since we don't want both processes to send the data with same sequence number)
        */

        sem_wait(sem);
        offset = lseek(fd, 0, SEEK_CUR);
        size_read = read(fd, sndPkt.payload, PACKET_SIZE);
        sem_post(sem);
        
        //EOF is reached, exit gracefully
        if(offset == file_size || size_read == 0)
        {
            //If parent, deallocate the shared resources
            if(pid > 0)
            {
                sem_unlink("semp");
                sem_close(sem);
            }
            exit(0);
        }

        //Now populate the data packet to be sent
        sndPkt.isData = 1;
        sndPkt.sizeOfPayload = PACKET_SIZE;
        sndPkt.seqNo = offset;
        if(pid == 0)
            sndPkt.channelID = 1;
        else
            sndPkt.channelID = 0;
        sndPkt.isLastPkt = 0;

        if(size_read != PACKET_SIZE)
        {
            //There was some problem in read()
            if(size_read < 0)
            {
                sem_unlink("semp");
                sem_close(sem);
                die("read()");
            }
            //Last chunk of the file (which is left to be transferred) is less than the PACKET_SIZE
            else if(size_read < PACKET_SIZE)
            {
                sndPkt.isLastPkt = 1;
                sndPkt.sizeOfPayload = size_read;
            }
        }

        //Save the packer for future use in cpySndPkt
        makeCopy(&cpySndPkt, sndPkt);
        bytesSent = send(skt, &sndPkt, sizeof(sndPkt), 0);

        if(bytesSent != sizeof(sndPkt))
        {
            sem_unlink("semp");
            sem_close(sem);
            die("send()");
        }
        
        printf("SENT PKT: Seq. No %d of size %d bytes from channel %d\n", 
        cpySndPkt.seqNo, cpySndPkt.sizeOfPayload, cpySndPkt.channelID);

        //Now keep waiting for ACK from the server
        numTrans = 0;
        while(1)
        {
            if(numTrans > MAX_ATTEMPTS)
            {
                printf("MAX-ATTEMPTS-FAILED: For pkt with Seq. No. %d\n", cpySndPkt.seqNo);
                break;
            }
            nready = poll(clients, 1, TIMEOUT);
            //Timeout occured
            if(nready == 0)
            {
                makeCopy(&tmp, cpySndPkt);

                //Send the data pkt again
                bytesSent = send(skt, &cpySndPkt, sizeof(cpySndPkt), 0);
                if(bytesSent != sizeof(tmp))
                {
                    sem_unlink("semp");
                    sem_close(sem);
                    die("send()");
                }
                printf("RESENT PKT: Seq. No %d of size %d bytes from channel %d\n", 
                tmp.seqNo, tmp.sizeOfPayload, tmp.channelID);
                makeCopy(&cpySndPkt, tmp);
                numTrans++;
                continue;
            }
            //Something received from the server
            else if(nready > 0)
            {
                memset((char *)&rcvPkt, 0, sizeof(rcvPkt));
                bytesRcvd = recv(skt, &rcvPkt, sizeof(rcvPkt), MSG_WAITALL);
                if(bytesRcvd <= 0)
                {
                    sem_unlink("semp");
                    sem_close(sem);

                    if(bytesRcvd == 0)
                        exit(0);
                    die("recv()");
                }
                //you got old ack, keep waiting
                if(rcvPkt.seqNo != sndPkt.seqNo)
                    continue;
                else    //you got the correct ack, move out of the loop
                    break;
            }
            else
            {
                die("poll()");
            }
            
        }
        
        if(numTrans > MAX_ATTEMPTS)
            continue;
        printf("RCVD ACK: For pkt with Seq. No %d from channel %d\n", 
        rcvPkt.seqNo, rcvPkt.channelID);

        //It is the ACK for the last pkt sent to the server, everything is done, exit
        if(rcvPkt.isLastPkt == 1)
        {
            sem_unlink("semp");
            sem_close(sem);
            exit(0);
        }
    }

    //Close the connection skt
    if(close(skt) < 0)
    {
        sem_unlink("semp");
        sem_close(sem);
        die("close(skt)");
    }

    //If parent process, deallocate shared resources
    if(pid > 0)
    {
        sem_unlink("semp");
        sem_close(sem);
        exit(0);
    }
}