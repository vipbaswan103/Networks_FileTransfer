----------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                METHODOLOGY
BASIC ARCHITECTURE:

1)  Client has been coded using fork(). The client process is forked to create 2 children. This creates two simultaneous connections i.e. channels

2)  Each child opens a TCP socket and requests connection from the server. So, two connection requests will be made to the server (in total).

3)  Multiple timers are used. Each child (created from the client process) runs its own timer. Therefore, each child will have its own timeout and retransmissions.
    So, each child acts like one channel.

4)  Both children share the same resource, the input file. 
    File descriptors along with read(), write() and lseek() system calls are used to handle the file.
    This is done becuase after forking, fd are "shared" between the two children. So, if one child reads the file, then second child will appropriately know from where to read (becuase fd will be common and changes will be visible to both).
    This prevents the client from sending two packets with same data.

5)  Seq Number is the first byte of the packet to be sent. It is found by using lseek() to find the current offset.
    NOTE: Now lseek() and read() must be done atomically to prevent client from sending packets with wrong seq nos. Hence, semaphores are used to guard these two operations.

6)  Server has been implemented using poll. Initially we poll on listening socket. If available, we accept the connection requests.
    NOTE: As connection requests are accepted, the number of polled sockets also increase.

CLIENT:
1)  Client reads a PACKET_SIZE block from the file. It then prepares the packet and sends it to the server. This will also help the client know whether its the last packet or not

2)  Then, the client polls for TIMEOUT time. 
    2.1)    If something is received before timeoute, it retrieved the pkt using recv().
            Then it checks the ACK of the packet and compares it with the seq no of the pkt it had sent. 
            If they aren't same, it ignore the ACK. Else, it accepts the pkt, puts it in the log and again goes to (1)
    
    2.2)    If timeout occurs, it retransmits the packet and again goes to (2)

SERVER:
1)  Server waits for some packet from the client using blocking recv()

2)  If something is received:
    2.1)    If bytes received is zero, the it means that FIN pkt has received. Server gracefully closes the connection.
    2.2)    If bytes received is non-zero, it checks whether this packet can be accomodated in the buffer. If not, it is rejected and server again goes to (1)
    2.3)    If pkt can be accomodated in the buffer, it inserts it in the buffer. Then, if needed, it empties the buffer by writing to the file.
            (See buffer section for more details)

3)  If both the clients have closed the connection, server also closes the connection

4)  Data will be available in output.txt

BUFFER:
1)  Buffer is an array of bufferEntry structure.
2)  Buffer is emptied in following cases:
    2.2)    Buffer is full. 
    2.3)    Last packet has arrived and all the packets before the last packet (in the buffer) have also arrived (buffer may or may not be full)
    2.3)    Server is about to close (It is done to flush the buffer, if something is still left, just for safety)
3)  Size of the buffer can be changed by changing the parameter BUFSIZE
----------------------------------------------------------------------------------------------------------------------------------------------------------------------

                                                
                                                
----------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                        HOW TO RUN
Please run the following commands in sequence:

1) make
2) ./server (In one instance of terminal)
3) ./client (In another instance of terminal)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------


----------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                        READING THE LOG
SENT:       Pkt has been sent
RESENT:     Pkt has been retransmitted (becuase of timeout)
RCVD:       Pkt has been received
DROPPED:    Pkt has been dropped by the server
REJECTED:   Pkt has been rejected (becuause it couldn't fit into the buffer)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------


----------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                            NOTE
1)  Ensure that file "input.txt" exists and user has read permission
2)  Sample input.txt is already present in the folder (199 Lines, 8667 characters)
3)  Refer to packet.h and comments of macros to change the parameters
4)  "output.txt" will be produced after program is run
----------------------------------------------------------------------------------------------------------------------------------------------------------------------