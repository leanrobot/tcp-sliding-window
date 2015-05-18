#include "UdpSocket.h"
#include "Timer.h"
#include "stdlib.h"
#include "stdio.h"
#include <random>

const int TIMEOUT_USEC = 1500;

bool isRandomDrop(int percent) {
    return rand() % 100 < percent;
}

bool canRecv(UdpSocket& sock) {
    return sock.pollRecvFrom() > 0;
}

int recvAck(UdpSocket& sock) {
    int ackNum;
    sock.recvFrom((char*)&ackNum, sizeof(int));
    return ackNum;
}

bool isTimeout(Timer& t) {
    return t.lap() >= TIMEOUT_USEC;
}

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

            while(timeout.lap() < TIMEOUT_USEC && !canRecv(sock)) {}

            // recv if available, otherwise count the retransmission.
            if(canRecv(sock)) {
                ackNum = recvAck(sock);
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
int clientSlidingWindow( UdpSocket &sock, const int max, int message[], int windowSize ) {
    bool* sent = new bool[max];
    for(int i=0; i<max; i++) sent[i] = false;

    int retransmitted   = 0;
    int base     = 0;
    int nextSeqNum = 0;
    Timer timer;

    while(nextSeqNum < max) {
        if(nextSeqNum < base + windowSize) {
            message[0] = nextSeqNum; // place sequence # in message[0].
            cerr << "send seq # = " << nextSeqNum << endl;
            sock.sendTo( (char*) message, MSGSIZE);

            // retransmission counter logic.
            if(sent[nextSeqNum]) retransmitted++;
            sent[nextSeqNum] = true;
            if(base == nextSeqNum)
                timer.start();
            nextSeqNum++;
        }
        else if(isTimeout(timer)) {
            nextSeqNum = base;
        }
        else if(canRecv(sock)) {
            int ack = recvAck(sock);
            base = ack + 1;
            if(base == nextSeqNum)
                timer.start();
        }
    }
    return retransmitted;
}

void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
          int windowSize, int dropPercent ) {
    cerr << "start window size = " << windowSize << endl;
    // init packets array all to false;
    bool* packets = new bool[max];
    for(int i=0; i<max; i++) packets[i] = false;

    int expectedSeqNum = 0;

    while(expectedSeqNum < max) {
        sock.recvFrom( (char*) message, MSGSIZE);
        int seqNum = message[0];

        if(!isRandomDrop(dropPercent)) {
            if(seqNum < max) packets[seqNum] = true;
            sock.ackTo((char*) &expectedSeqNum, sizeof(int));

            // print current state to STDERR.
            cerr << "ACK " << expectedSeqNum << " received = " << seqNum << endl;
            if(seqNum+1 != expectedSeqNum) cerr << "\t\tNOT EXPECTED" << endl;
            else cerr << endl;

            if(seqNum == expectedSeqNum) {
                expectedSeqNum++;
            }
        }
    }

    // for(int i=0; i<max;) {
    //     // receive and recorded a message.
    //     sock.recvFrom( (char*) message, MSGSIZE);
    //     int seqNum = message[0];
        
    //     bool isDrop = isRandomDrop(dropPercent);
    //     // randomly drop (not record) a received packet.
    //     if(!isDrop) {
    //         if(seqNum < max) packets[seqNum] = true;

    //         // fast forward before ack'ing
    //         while(packets[i]) i++;

    //         sock.ackTo((char*) &i, sizeof(int));
    //         // print current state to STDERR.
    //         cerr << "ACK " << i << " received = " << seqNum << endl;
    //         if(seqNum+1 != i)   cerr << "\t\tNOT EXPECTED" << endl;
    //         else                cerr << endl;
    //     }

    // }

    delete packets;

    cerr << "finish window size = " << windowSize << endl;
}
/*
int clientSlidingWindow( UdpSocket &sock, const int max, int message[], int windowSize ) {
    // used to track retransmission.
    bool* sent = new bool[max];
    for(int i=0; i<max; i++) sent[i] = false;

    int retransmitted   = 0;
    int windowStart     = 0;
    int curSeq          = 0;
    Timer timer;

    while(windowStart < max) {
        // state: inside window & not at end of transmission.
        while(curSeq < windowStart + windowSize && curSeq < max) {
            message[0] = curSeq; // place sequence # in message[0].
            cerr << "send seq # = " << curSeq << endl;
            sock.sendTo( (char*) message, MSGSIZE);

            // retransmission counter logic.
            if(sent[curSeq]) retransmitted++;
            sent[curSeq] = true;

            curSeq++;
        }

        // state: wait for the window to move, or timeout
        while( (curSeq >= windowStart + windowSize) || (curSeq == max && canRecv(sock)) ) {
            timer.start();

            // wait for an event to happen.
            while(timer.lap() < TIMEOUT_USEC && !canRecv(sock)) {}
            
            // state: receive acks
            if(canRecv(sock)) {
                while(canRecv(sock)) {
                    int ackNum = recvAck(sock);
                    if(ackNum > windowStart) {
                        windowStart = ackNum;
                        cerr << "ACK window start = " << windowStart << endl;
                    }
                }
            } 
            // state: timeout, resend window.
            else {
                cerr << "timeout: retransmitting window (start = "
                    << windowStart <<" )\n";
                curSeq = windowStart;
            }
        }
    }
    return retransmitted;
}


void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
          int windowSize, int dropPercent ) {
    cerr << "start window size = " << windowSize << endl;
    // init packets array all to false;
    bool* packets = new bool[max];
    for(int i=0; i<max; i++) packets[i] = false;

    for(int i=0; i<max;) {
        // receive and recorded a message.
        sock.recvFrom( (char*) message, MSGSIZE);
        int seqNum = message[0];
        
        bool isDrop = isRandomDrop(dropPercent);
        // randomly drop (not record) a received packet.
        if(!isDrop) {
            if(seqNum < max) packets[seqNum] = true;

            // fast forward before ack'ing
            while(packets[i]) i++;

            sock.ackTo((char*) &i, sizeof(int));
            // print current state to STDERR.
            cerr << "ACK " << i << " received = " << seqNum << endl;
            if(seqNum+1 != i)   cerr << "\t\tNOT EXPECTED" << endl;
            else                cerr << endl;
        }

    }

    delete packets;

    cerr << "finish window size = " << windowSize << endl;
}
*/