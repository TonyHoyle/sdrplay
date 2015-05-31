#ifndef __MYSOCKET__H_
#define __MYSOCKET__H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

class mySocket;

class socketEvent
{
public:
    virtual bool clientConnected(mySocket *socket) = 0;
    virtual void packetReceived(mySocket *socket, const void *packet, ssize_t packetLen, sockaddr *sender, socklen_t senderLen) = 0;
};

class mySocket
{
private:
    int mSocket4, mSocket6, mConnectedSocket;
    socketEvent *mHandler;
    bool mExit;
    int mPort;
    bool mTcp;
    bool mMulticast;
    int mInterfaceId;
    sockaddr_storage mEndpoint;
    sockaddr_in mMulticastV4;
    sockaddr_in6 mMulticastV6;
    socklen_t mEndpointLen;
    char mEndpointName[NI_MAXHOST];

    bool read(int socket);
    static void *_run_tcp(void *ptr);
    void run_tcp();

public:
    mySocket();
    virtual ~mySocket();
    void init(socketEvent *handler, bool tcp);
    void init(int connectedSocket, mySocket *parentSocket);

    const char *interfaceAddress(int af, const char *interfaceName);
    const char *endpointAddress();

    void bind(int port, bool ipv4 = true, bool ipv6 = true);
    void bindMulticast(int port, const char *v4Address, const char *v6Address);
    void listen();
    mySocket *connect(const char *address, int port);
    mySocket *connect(const sockaddr *to, socklen_t toLen);
    ssize_t send(const void *packet, size_t packetLen);
    ssize_t sendTo(const void *packet, size_t packetLen, const sockaddr *to, socklen_t toLen);
    ssize_t replyTo(const void *packet, size_t packetLen, const sockaddr *to, socklen_t toLen);
};

#endif