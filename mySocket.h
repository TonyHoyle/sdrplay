#ifndef __MYSOCKET__H_
#define __MYSOCKET__H_

#include <sys/socket.h>

class socketEvent
{
public:
    virtual void packetReceived(const char *packet, ssize_t packetLen, sockaddr *sender, socklen_t senderLen) = 0;
};

class mySocket
{
private:
    int mSocket4, mSocket6, mConnectedSocket;
    socketEvent *mHandler;
    bool mExit;
    const char *mAddress;
    int mPort;
    bool mTcp;
    bool mMulticast;
    const char *mInterfaceName;
    const char *mInterface4, *mInterface6;
    int mInterfaceId;
    sockaddr_storage mEndpoint;
    socklen_t mEndpointLen;
    char mEndpointName[NI_MAXHOST];

    void read(int socket);
    void lookupInterface();

    mySocket(int connectedSocket, mySocket *parentSocket);
public:
    mySocket(socketEvent *handler, const char *interface, bool tcp);
    virtual ~mySocket();

    const char *interfaceAddress(int af);
    const char *endpointAddress();

    void bind(const char *address, int port, bool ipv4, bool ipv6, bool multicast);
    void listen();
    mySocket *connect(const char *address, int port);
    mySocket *connect(const sockaddr *to, socklen_t toLen);
    ssize_t send(const char *packet, size_t packetLen);
    ssize_t sendTo(const char *packet, size_t packetLen, const sockaddr *to, socklen_t toLen);
    ssize_t replyTo(const char *packet, size_t packetLen, const sockaddr *to, socklen_t toLen);
};

#endif