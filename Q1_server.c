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
    // printf("insertInBuffer: %p\n", buffer);
    int i = ((pkt->seqNo)-startOffset)/(PACKET_SIZE);
    buffer[i].isFilled = 1;
    buffer[i].sizeOfPayload = pkt->sizeOfPayload;
    // strcpy(buffer[i].payload, pkt->payload);
    copyString(buffer[i].payload, pkt->payload);
    // printf("index = %d   Was here\n", i);
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
int insertInFile(bufferEntry * buffer, int fd, int *startOffset, int *endOffset, int doWrite)
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
            return fd;
        }
        for(int i=0; i<BUFSIZE; i++)
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
            (*endOffset) = (*startOffset) + (BUFSIZE)*PACKET_SIZE - 1;
        }
    }
    return fd;
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
    //Open the skt that will be used for listening
    if((listenSkt = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        die("socket()\n");
    
    //Bind the socket to server's IP and PORT
    if(bind(listenSkt, (struct sockaddr*)&si_me, slen) == -1)
        die("bind()\n");
    
    //Mark the port as listerning for connections
    if(listen(listenSkt, MAXPENDING) == -1)
        die("listen()\n");
    

    //For synch, semaphores need to be defined
    //Becuase of fork(), we'll need to put semaphore in shared memory
    key_t shmkey, shmkeyOffset;
    int shmid, shmidOffset;
    sem_t *sem;
    int * offsets;
    bufferEntry *buffer;

    //NOTE: Althought /dev/null exists on almost all UNIX systems, please verify once that it exists
    shmkey = ftok("/dev/null", FTOK_KEY1);
    // printf("%d\n", shmkey);
    if(shmkey < 0)
        die("ftok() 1");
    shmkeyOffset = ftok("/dev/null", FTOK_KEY2);
    if(shmkeyOffset < 0)
        die("ftok() 2");
    
    //Either the shared memory already exists (probably program crashed and couldn't detach it) or some other error
    shmid = shmget(shmkey, sizeof(bufferEntry)*BUFSIZE, 0644 | IPC_CREAT);
    if(shmid < 0)
    {
        // //Memory exists
        // if(errno == EEXIST)
        // {
        //     //memory
        //     printf("EXISTS 1\n");
        //     shmid = shmget(shmkey, 0, IPC_CREAT);
        //     if(shmid < 0)
        //         die("shmget(2)");
        // }
        // else
        die("shmget()");
    }
    shmidOffset = shmget(shmkeyOffset, sizeof(int)*2, 0644 | IPC_CREAT);
    if(shmidOffset < 0)
    {
        // //Memory exists
        // if(errno == EEXIST)
        // {
        //     //Get fresh memory 
        //     printf("EXISTS 2\n");
        //     shmidOffset = shmget(shmkeyOffset, sizeof(int)*2, 0644 | IPC_CREAT);
        //     if(shmidOffset < 0)
        //         die("shmget(2)");
        // }
        // else
        die("shmget()");
    }
    //Get the pointer to memory in shdata
    buffer = (bufferEntry *)shmat(shmid, NULL, 0);
    offsets = (int *)shmat(shmidOffset, NULL, 0);
    //Now, we are ready to initialize our shared semaphore with initial value 1
    
    sem = sem_open ("sempSer", O_CREAT, 0644, 1); 
    if(sem == NULL)
    {
        //Semaphore with same name "sempServer" already exists
        if(errno == EEXIST)
        {
            //Unlink the semaphore
            shmdt(buffer);
            shmctl(shmid, IPC_RMID, 0);
            shmdt(offsets);
            shmctl(shmidOffset, IPC_RMID, 0);
            sem_unlink("sempSer");
            sem_close(sem);

            //Now, create a fresh semaphore
            sem = sem_open ("sempSer", O_CREAT, 0644, 1); 
        }
        else
            die("sem_open()");
    }

    sem_wait(sem);
    initializeBuffer(buffer, &offsets[0], &offsets[1]);
    sem_post(sem);
    //fork() to create two processes
    //Now each process will handle one client
    pid = fork();

    //Fork failed, release resources, exit
    if(pid < 0)
    {
        sem_unlink("sempSer");
        sem_close(sem);
        die("fork()\n");
    }

        /*          
        *          NOTE: From this point onwards,
        *          1) All code executes for both child and the parent
        *          2) Server closes only if both the clients close the connection
        */


    //Each process waits for a connection request
    connSkt = accept(listenSkt, (struct sockaddr*)&si_other, &clen);
    if(clen < 0)
    {
        shmdt(buffer);
        shmctl(shmid, IPC_RMID, 0);
        shmdt(offsets);
        shmctl(shmidOffset, IPC_RMID, 0);
        sem_unlink("sempSer");
        sem_close(sem);
        die("accept()\n");
    }

    //Now close the listen socket for each process (since we need to handle only 2 clients)
    if(close(listenSkt) < 0)
    {
        shmdt(buffer);
        shmctl(shmid, IPC_RMID, 0);
        shmdt(offsets);
        shmctl(shmidOffset, IPC_RMID, 0);
        sem_unlink("sempSer");
        sem_close(sem);
        die("close(listenSkt)\n");
    }

    //For each process, do this (We'll break only if both the clients have closed the connection)
    while(1)
    {
        memset((char *)&rcvPkt, 0, sizeof(rcvPkt));

        //Wait for data pkt from the connected channel
        bytesRcvd = recv(connSkt, &rcvPkt, sizeof(rcvPkt), 0);

        //If bytesRcvd is 0, then connection has been gracefully closed by the client
        //Else, some error occured in recv()
        if(bytesRcvd <= 0)
        {
            if(bytesRcvd == 0)
            {
                sem_wait(sem);
                fd = insertInFile(buffer, fd, &offsets[0], &offsets[1], 1);
                sem_post(sem);
            }
            shmdt(buffer);
            shmctl(shmid, IPC_RMID, 0);
            shmdt(offsets);
            shmctl(shmidOffset, IPC_RMID, 0);
            sem_unlink("sempSer");
            sem_close(sem);
            if(bytesRcvd == 0)
            {
                break;
            }
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

        //offset is where to write the data
        offset = rcvPkt.seqNo;


        //(Similar to client)
        sem_wait(sem);
        if(checkCompatibility(offsets[0], offsets[1], rcvPkt.seqNo) == 1)
        {
            // insertInBuffer(buffer, offsets[0], &rcvPkt);
            // fd = insertInFile(buffer, fd, &offsets[0], &offsets[1], rcvPkt.isLastPkt);
            lseek(fd, offset, SEEK_SET);
            write(fd, rcvPkt.payload, rcvPkt.sizeOfPayload);
        }
        else if(rcvPkt.seqNo > offsets[1])
        {
            printf("PKT REJECTED: Seq. No %d of size %d bytes from channel %d\n",
            rcvPkt.seqNo, rcvPkt.sizeOfPayload, rcvPkt.channelID);
            sem_post(sem);
            continue;
        }
        sem_post(sem);

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
        {
            shmdt(buffer);
            shmctl(shmid, IPC_RMID, 0);
            shmdt(offsets);
            shmctl(shmidOffset, IPC_RMID, 0);
            sem_unlink("sempSer");
            sem_close(sem);
            die("send()\n");
        }

        printf("SENT ACK: For PKT with Seq. No. %d from channel %d\n",
        sndPkt.seqNo, sndPkt.channelID);
    }
    
    //Close the connection socket
    if(close(connSkt) < 0)
    {
        shmdt(buffer);
        shmctl(shmid, IPC_RMID, 0);
        shmdt(offsets);
        shmctl(shmidOffset, IPC_RMID, 0);
        sem_unlink("sempSer");
        sem_close(sem);
        die("close(connSkt)\n");
    }

    //If parent, then deallocate shared resources
    if(pid > 0)
    {
        shmdt(buffer);
        shmctl(shmid, IPC_RMID, 0);
        shmdt(offsets);
        shmctl(shmidOffset, IPC_RMID, 0);
        sem_unlink("sempSer");
        sem_close(sem);
        exit(0);
    }
}