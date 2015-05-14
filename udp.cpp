#include "UdpSocket.h"
#include "Timer.h"

const int TIMEOUT_USEC = 1500;

int clientStopWait( UdpSocket &sock, const int max, int message[] ) {
    cerr << "client: stop & wait test:" << endl;

    int retransNum = 0;

    int ackNum = -1;
    for ( int i = 0; i < max; i++ ) {
        message[0] = i; // place sequence # in message[0].

        while(ackNum != i) {
            sock.sendTo( ( char * )message, MSGSIZE ); // send the message

            Timer timeout;
            timeout.start();

            while(timeout.lap() < 1500 && sock.pollRecvFrom() <= 0) {}

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

    int ackNum = -1;
    for ( int i = 0; i < max; i++ ) {
        while(ackNum != i) {
            sock.recvFrom( ( char * ) message, MSGSIZE );   // udp message receive
            ackNum = message[0];
        }
        sock.ackTo( (char*) &ackNum, sizeof(int));
        cerr << "ack " << ackNum << endl;
    }
}

int clientSlidingWindow( UdpSocket &sock, const int max, int message[], 
int windowSize ) {
    return -1;
}

void serverEarlyRetrans( UdpSocket &sock, const int max, int message[], 
          int windowSize ) {
    
}
