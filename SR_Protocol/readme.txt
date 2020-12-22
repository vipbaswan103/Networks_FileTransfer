--------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                    METHODOLOGY

BASIC ARCHITECTURE:
1)  Client sends packets over a window of size WINDOW_SIZE. A common timer is run for whole window.
    Polling is done on the client side to handle the common timer.

2)  A single .c file is created for the relay. To run two instances of the relay, different command line args need to be provided (described later)
    
3)  Server receieves data from the relays and sends the acks to the relays.

4)  Since no forking is done at client/server/relays, we don't need to synchronize the handling of shared resources like input file and buffer.

5)  read(), write() and lseek() are used for file handling

6)  Buffering is done on the server side with buffer size defined as BUFFERSIZE

FEATURES:
1)  Since we are developing this reliable protocol on top of UDP, I have handled packet losses using timeout.
    
2)  Though chances of ACK loss are pretty low, their loss have also been handled (using timeout only. If ACK gets lost, timeout occurs on client side, and packet is retransmitted.)
    See 2.2 in SERVER for more details.

3)  Premature timeout (though chances are low) has been handled. See 2.2 in SERVER for more details.

4)  Server and relays are passive listeners. Server keeps on listening for packet transfers from client side.
    Server will detect successfull transfer and give a message on STDOUT (All packets received successfully. Check output.txt!!) when it has received all the packets successfully from the client (indicating that output.txt has been created)
    
5)  Client closes when file has been successfully transferred.
    It also displays the msg: File transferred successfully to the server
    
6)  Log generation and sorting have been handled.
    (Please read NOTE before running the program)

7)  Handles the case when file size isn't multiple of packet size

CLIENT:
1)  Client reads data of size PACKET_SIZE from the file. Prepares a pkt and sends it to appropriate relay.
    This is done maximum of WINDOW_SIZE times (if no more data is there, number of pkts sent are less than or equal to WINDOW_SIZE).

2)  Now that the client has sent all the packets from the window, it polls to recieve ACKs for all the packets in the window

3)  An array(1/0) of WINDOW_SIZE is maintained which shows which packets have received their ACKs.

4)  Polling is done with timout of TIMEOUT ms. 
    4.1)    Now, if there is something to receive:
            4.2)    Server may have sent multiple ACKs. But we don't know how many. So, we again poll "but" with timeout of 0 ms.
                    This essentially means that if somedata is there to be read, client reads the packet and marks it as ACKed in the array 
            4.3)    If the pollout with timeout 0ms timesout, it means we have read everything that was sent by server till now. 
                    Now check once that whether all packets in the window have been ACKed or not. If yes, got back to (1) else go back to (4)

    4.2)    There isn't anything to receive. Timout of TIMEOUT ms occurs.
            Now scan the array prepared in (3), and retransmit all those packets who are still unACKed. Go back to (4)

5) Eventually, all ACKs will arrive and we go back to (1). If file has been completely transferred, exit

SERVER:
1)  Server waits for data using blocking recvfrom() call.
2)  If something is received:
    2.1)    Check the seq number to know whether it can fit in the buffer. If yes, put it in the buffer (see BUFFER section for more details)
    2.2)    If pkt can't fit in the buffer, it is rejected. 
            NOTE: If we get a PKT with seq No less than the buffer's lower offset, it means that ACK has been lost/overly delayed. Send the ACK again so that client's window doesn't get blocked.
    2.3)    If pkt is inserted into the buffer, send an ACK (to the node from whom server received the pkt)
3)  If last packet has been received and all packets before it have also been received, then file has been successfully transferred.
    NOTE:   Server will now prompt the user that file has been transferred successfully.

RELAY:
1)  Relay node waits for data using blocking recvfrom() call.
2)  If something is received:
    2.1)    First, it is checked whether the pkt received is data pkt or not. If its data pkt, it is checked whether it should be dropped or not (based on PDR). 
            If it is dropped, go back to (1)
    2.2)    Now, a random number, randNum, is generated from 0 to 2 (both inclusive). Then, process sleeps for randNum ms. This introduces delay for the packets.
    2.3)    Then, the pkt is routed to appropriate next hop using the "dest" field in pkt received.
    2.4)    Go back to (1)

BUFFER:
1)  Buffer is maintained using an array of structures.
2)  Every pkt that is received by the server is checked for compatibility in the buffer (whether it can fit in the buffer or not).
3)  If it can fit in the buffer, it is inserted in the buffer
4)  Buffer is emptied in the following cases:
    4.1)    Buffer is full
    4.2)    Last pkt has arrived and all the packets before the last packet have also arrived (buffer may or may not be full)


--------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                HOW TO RUN

NOTE: Please see NOTE at the end of readme before running

Run following commands in sequence (Please ensure that previous running session of server and relay nodes are closed)
1)  make
2)  ./server
3)  ./relay 1   (To run relay node 1)
4)  ./relay 2   (To run relay node 2)
5)  ./client

Run 2-5 on separate terminal instances.

To see how to get the complete log in sorted order of timestamp, see NOTE at the end of readme
--------------------------------------------------------------------------------------------------------------------------------------------------------------------


--------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                READING THE LOG
Events:
S       Pkt sent
R       Pkt received
D       Pkt dropped (done at the relay nodes)
TO      Timeout occurs (at client)
RE      Retransmission of pkt (at client)
REJ     Pkt rejected (at server, because it couldn't fit into the buffer)
--------------------------------------------------------------------------------------------------------------------------------------------------------------------


--------------------------------------------------------------------------------------------------------------------------------------------------------------------
                                                NOTE

1)  Input file "input.txt" must exist and read permission must be provided to user. 
    Sample input.txt (236 lines, 10171 characters) has already been provided in the folder.

2)  File "output.txt" will be created as output of the transfer. If it exists before program is run, then it is trucated to 0 bytes, and it is filled freshly. If it doesn't exist, server creates it.

3)  Note that server and relays are passive listeners. 
    In accordance with this philosophy, server (though it detects when file has been successfully transferred and displays a msg on STDOUT) and relays keep on listening even when client exits.
    **So, to transfer a new file, please close the server and relays (using Ctrl + C) and run the steps 2-5 of HOW TO RUN again**
    This prevents log from multiple sessions being collected in the same log file and facilitates generation of fresh output.txt file for each new transfer

3)  Output of the program will produce following files:
    3.1)    output.txt          :   Contains the transferred input.txt
    3.2)    log_server.txt      :   Contains the server log (in sorted order of timestamps)
    3.2)    log_client.txt      :   Contains the client log (in sorted order of timestamps)
    3.2)    log_relay1.txt      :   Contains the relay node 1 log (in sorted order of timestamps)
    3.2)    log_relay2.txt      :   Contains the relay node 2 log (in sorted order of timestamps)

4)  To produce complete log file (log.txt) in sorted order of timestamps, run the following script (this script appends the logs of all the files and then sorts):
    ./getLog.sh
    User must have permission to run .sh 

    If you aren't able to run the above shell script for any reasons, then run the following commands in order

    4.1)    cat log_server.txt log_client.txt log_relay1.txt log_relay2.txt > logTmp.txt
    4.2)    sort -k3 logTmp.txt > log.txt
    4.3)    rm logTmp.txt
    4.4)    cat log.txt

    Either way, you'll have the log printed on termianl and also present in the log.txt file
--------------------------------------------------------------------------------------------------------------------------------------------------------------------