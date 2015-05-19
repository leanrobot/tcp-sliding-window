#include "UdpSocket.h"
#include "Timer.h"
#include "stdlib.h"
#include "stdio.h"
#include <cstdlib>

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

    int retransmission = 0;
    int ackNum = -1;
    Timer timeout;

    for ( int i = 0; i < max; i++ ) {
        message[0] = i; // place sequence # in message[0].

        while(ackNum != i) {
            sock.sendTo( ( char * )message, MSGSIZE ); // send the message

            timeout.start();

            while(!isTimeout(timeout) && !canRecv(sock)) {}

            // recv if available, otherwise count the retransmission.
            if(canRecv(sock)) {
                ackNum = recvAck(sock);
            } else {
                retransmission++;
                // cerr << "timeout: retransmitting " << i << endl;
            }
        }

        // cerr << "ack = " << ackNum << " message = " << message[0] << endl;
    }
    return retransmission;
}

void serverReliable( UdpSocket &sock, const int max, int message[] ) {
    cerr << "server reliable test:" << endl;

    for ( int i = 0; i < max; i++ ) {
        int ackNum;
        do {
            sock.recvFrom( ( char * ) message, MSGSIZE );
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
    // used to track messages that were already sent.
    bool sent[max];
    for(int i=0; i<max; i++) sent[i] = false;

    int retransmitted   = 0;
    int base            = 0; // start of the window
    int nextSeqNum      = 0; // expected sequence number.
    Timer timer;

    while(nextSeqNum < max || base < max) {
        // fprintf(stderr, "window = %d, base = %d, nextSeqNum = %d, base+windowSize = %d\n", windowSize, base, nextSeqNum, base+windowSize);

        // in window & not finished transmitting.
        if(nextSeqNum < base + windowSize && nextSeqNum < max) {
            message[0] = nextSeqNum; // place sequence # in message[0].
            // cerr << "send seq # = " << nextSeqNum << endl;
            sock.sendTo( (char*) message, MSGSIZE);

            // if the packets has already been sent, count as a retransmission.
            if(sent[nextSeqNum]) retransmitted++;
            sent[nextSeqNum] = true;

            nextSeqNum++;
        }
        // outside window
        else {
            timer.start();
            bool windowMoved = false;

            // wait for either a timeout or an ack recv.
            while(!isTimeout(timer) & !canRecv(sock)) {}

            // ack received.
            if(canRecv(sock)) {
                int ack = recvAck(sock);
                // cerr << "receive ACK " << ack << endl;
                if(ack > base) {
                    base = ack;
                    windowMoved = true;
                }
            }
            // timeout
            else {
                if(!windowMoved) {
                    // cerr << "timeout base = " << base << endl;
                    nextSeqNum = base;
                }
            }
        }
    }
    return retransmitted;
}

void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
          int windowSize, int dropPercent) {
    cerr << "server: early retransmit test:" << endl;
    fprintf(stderr, "start window size = %d, drop percent = %d\n", windowSize, dropPercent);
    // used to track all received packets.
    bool packets[max];
    for(int i=0; i<max; i++) packets[i] = false;

    int expectedSeqNum = 0;

    while(expectedSeqNum < max) {
        sock.recvFrom( (char*) message, MSGSIZE);
        int seqNum = message[0];

        // fprintf(stderr,"window = %d, seqNo = %d, received = %d\n", windowSize, expectedSeqNum, seqNum);
        if(!isRandomDrop(dropPercent)) {
            packets[seqNum] = true;
            if(seqNum == expectedSeqNum) {
                // fast forward expectedSeqNum to be the 
                //     next unreceived (false) packet.
                while(expectedSeqNum < max && packets[expectedSeqNum])
                    expectedSeqNum++;
            }

            int ackNum = expectedSeqNum;
            // ack a valid packet.
            if(ackNum <= max) {
                sock.ackTo((char*) &ackNum, sizeof(ackNum));
            }
        }
    }

    fprintf(stderr, "end window size = %d, drop percent = %d\n", windowSize, dropPercent);
}
