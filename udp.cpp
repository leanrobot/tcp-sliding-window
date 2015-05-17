#include "UdpSocket.h"
#include "Timer.h"
#include "stdlib.h"
#include "stdio.h"

const int TIMEOUT_USEC = 1500;

/*==============================================================================
        Stop & Wait Implementation
*/

int clientStopWait( UdpSocket &sock, const int max, int message[] ) {
    cerr << "client: stop & wait test:" << endl;

    int retransNum = 0;

    int ackNum = -1;
    Timer timeout;
    for ( int i = 0; i < max; i++ ) {
        message[0] = i; // place sequence # in message[0].

        while(ackNum != i) {
            sock.sendTo( ( char * )message, MSGSIZE ); // send the message

            timeout.start();

            while(timeout.lap() < TIMEOUT_USEC && sock.pollRecvFrom() <= 0) {}

            // recv if available, otherwise count the retransmission.
            if(sock.pollRecvFrom() > 0) {
                sock.recvFrom((char*)&ackNum, sizeof(int));
            } else {
                retransNum++;
                cerr << "timeout: retransmitting " << i << endl;
            }
        }

        cerr << "ack = " << ackNum << " message = " << message[0] << endl;
    }
    return retransNum;
}

void serverReliable( UdpSocket &sock, const int max, int message[] ) {
    cerr << "server reliable test:" << endl;

    for ( int i = 0; i < max; i++ ) {
        int ackNum;
        do {
            sock.recvFrom( ( char * ) message, MSGSIZE );   // udp message receive
            ackNum = message[0];
        } while(ackNum != i);
        sock.ackTo( (char*) &ackNum, sizeof(int));
        cerr << "ack " << ackNum << endl;
    }
}

/*==============================================================================
        Sliding Window Implementation
*/
/*
int clientSlidingWindow( UdpSocket &sock, const int max, int message[], int windowSize ) {
    Timer timeout;
    int retransmitted = 0;

    // create an array to track sent packets.
    bool* sent = new bool[max];
    for(int i=0; i<max; i++) sent[i] = false;

    int winStart = 0;
    // int nack = 0;
    for(int i=0; i<max; i++) {
        // if(i==500 && windowSize == 5) {
        //     cout << "bug introduced\n";
        //     i++;
        // }
        // if(nack < 0) nack = 0;
        // check if the message has been sent before.
        if(i<winStart || i > winStart+windowSize) {
            cout << "ERROR with window. i = " << i << " winstart = " <<
             winStart << " windowsize = " << windowSize << endl;
        }
        if(sent[i]) retransmitted++;

        message[0] = i; // place sequence # in message[0].
        cerr << "send seq # = " << i << endl;
        sock.sendTo( (char*) message, MSGSIZE);
        // nack++;

        // register that the message was sent for the first time
        sent[i] = true;

        // if(sock.pollRecvFrom() > 0) {
        //     int nextSeqNum;
        //     sock.recvFrom((char*)&nextSeqNum, sizeof(int));

        //     if(nextSeqNum > winStart) {
        //         winStart = nextSeqNum;
        //     }
        // }

        while(i >= winStart+windowSize) {
            timeout.start();
            // wait for data
            while(timeout.lap() < TIMEOUT_USEC && sock.pollRecvFrom() <= 0) {}
            // receive acks
            if(sock.pollRecvFrom() > 0) {
                int nextSeqNum;
                sock.recvFrom((char*)&nextSeqNum, sizeof(int));
                if(nextSeqNum > winStart) {
                    // nack -= nextSeqNum - winStart;
                    winStart = nextSeqNum;
                }
                // cerr << "nextSeqNum = " << nextSeqNum << endl;
            } else { // resend the window.
                i = winStart;
                // nack = 0;

                cerr << "window timeout" << endl;
                cerr << "i = " << i << " winStart = " << winStart << 
                    " winSize = " << windowSize << endl;

             }
        }
    }
    return retransmitted;
}
*/

bool canRecv(UdpSocket& sock) {
    return sock.pollRecvFrom() > 0;
}

int recvAck(UdpSocket& sock) {
    int ackNum;
    sock.recvFrom((char*)&ackNum, sizeof(int));
    return ackNum;
}

int clientSlidingWindow( UdpSocket &sock, const int max, int message[], int windowSize ) {
    int retransmitted = 0;
    int windowStart = 0;
    int curSeq = 0;
    // int ackedSeq = 0;
    Timer timer;

    while(windowStart < max) {
        // state: inside window
        while(curSeq < windowStart + windowSize && curSeq < max) {
            message[0] = curSeq; // place sequence # in message[0].
            cerr << "send seq # = " << curSeq << endl;
            sock.sendTo( (char*) message, MSGSIZE);
            curSeq++;
        }

        // printf("windowstart = %d, curSeq = %d, windowsize = %d\n", windowStart, curSeq, windowSize);
        cerr << "loop" << endl;
        // state: wait for the window to move, or timeout
        while(curSeq >= windowStart + windowSize || curSeq == max && canRecv(sock) ) {
            timer.start();

            // wait for an event to happen.
            while(timer.lap() < TIMEOUT_USEC && !canRecv(sock)) {}
            
            // state: receive acks
            if(canRecv(sock)) {
                cerr << "recv" << endl;    
                while(canRecv(sock)) {
                    int ackNum = recvAck(sock);
                    if(ackNum > windowStart) {
                        windowStart = ackNum;
                    }
                }
            } 
            // state: timeout, resend window.
            else {
                cerr << "timeout" << endl;
                // printf("windowstart = %d, curSeq = %d, windowsize = %d\n", windowStart, curSeq, windowSize);
                curSeq = windowStart;
            }
        }

        // // state: finishing the window.
        // while(curSeq == max) {
        //     int ackNum = recvAck(sock);
        //     if(ackNum > windowStart) {
        //         windowStart = ackNum;
        //     }
        //     if(windowStart == max) break;
        // }
    }
}

void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
          int windowSize ) {
    cerr << "start window size = " << windowSize << endl;
    // init packets array all to false;
    bool* packets = new bool[max];
    for(int i=0; i<max; i++) packets[i] = false;

    for(int i=0; i<max;) {
        // receive and recorded a message.
        sock.recvFrom( (char*) message, MSGSIZE);
        int seqNum = message[0];
        // cerr << "receive seq # = " << seqNum << endl;
        
        packets[seqNum] = true;

        // fast forward before ack'ing
        while(packets[i]) i++;

        // ack the next expected.
        sock.ackTo((char*) &i, sizeof(int));
        cerr << "ACK " << i << " received = " << seqNum;
        if(seqNum+1 != i) cerr << "     NO MATCH" << endl;
        else cerr << endl;
        // cerr << "i = " << i << endl;
    }

    bool allAcked = true;
    for(int i=0; i<max; i++) {
        allAcked = allAcked && packets[i];
    }
    cerr << "allAcked? " << allAcked << endl;
    delete packets;

    cerr << "finish window size = " << windowSize << endl;
}
