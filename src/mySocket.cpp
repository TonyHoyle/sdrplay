#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <string.h>
#include <pthread.h>

#include "error.h"
#include "mySocket.h"

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
//#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

#ifndef NI_IDN
#define NI_IDN 0
#endif

mySocket::mySocket()
{
    mHandler = NULL;
    mSocket4 = -1;
    mSocket6 = -1;
    mConnectedSocket = -1;
    mEndpointLen = 0;
}

void mySocket::init(socketEvent *handler, bool tcp)
{
    mHandler = handler;
    mTcp = tcp;
}

void mySocket::init(int connectedSocket, mySocket *parentSocket)
{
    mTcp = true;
    mConnectedSocket = connectedSocket;
    mHandler = parentSocket->mHandler;
    mEndpoint = parentSocket->mEndpoint;
    mEndpointLen = parentSocket->mEndpointLen;
}

mySocket::~mySocket()
{
    if(mSocket4 > 0)
        close(mSocket4);
    if(mSocket6 > 0)
        close(mSocket6);
    if(mConnectedSocket > 0)
        close(mConnectedSocket);
}

void mySocket::bindMulticast(int port, const char *v4Address, const char *v6Address)
{
    int no = 0;
    ipv6_mreq mreq6;
    ip_mreq mreq;
    addrinfo hints = {0};
    addrinfo *ai;

    bind(port, v4Address != NULL, v6Address != NULL);

    mMulticast = true;

    if(v4Address) {
        if (setsockopt(mSocket4, IPPROTO_IP, IP_MULTICAST_LOOP, &no, sizeof(no)) < 0)
            throw new error("Unable to set IP_MULTICAST_LOOP");

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(v4Address, NULL, &hints, &ai) < 0)
            throw new error("Unable to get ipv4 multicast address");

        memcpy(&mMulticastV4, ai->ai_addr, ai->ai_addrlen);

        mreq.imr_multiaddr.s_addr = mMulticastV4.sin_addr.s_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if (setsockopt(mSocket4, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            throw new error("ipv4 group membership failed");
    }

    if(v6Address) {
        if (setsockopt(mSocket6, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &no, sizeof(no)) < 0)
            throw new error("Unable to set IPV6_MULTICAST_LOOP");

        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(v6Address, NULL, &hints, &ai) < 0)
            throw new error("Unable to get ipv6 multicast address");

        mreq6.ipv6mr_interface = 0;
        mreq6.ipv6mr_multiaddr = mMulticastV6.sin6_addr;

        if (setsockopt(mSocket6, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0)
            throw new error("ipv6 group membership failed");
    }
}

void mySocket::bind(int port, bool ipv4, bool ipv6)
{
    int yes = 1;
    addrinfo *ai;
    addrinfo hints = {0};
    char port_s[32];
    int sockType;

    if(mConnectedSocket > 0)
        throw new error("Can't bind on connected mySocket");

    sprintf(port_s,"%d", port);
    mPort = port;
    mMulticast = false;

    if(mTcp) sockType = SOCK_STREAM;
    else sockType = SOCK_DGRAM;

    if(ipv4)
        mSocket4 = ::socket(AF_INET, sockType, 0);

    if(ipv6)
        mSocket6 = ::socket(AF_INET6, sockType, 0);

    if(mSocket4 <= 0 && mSocket6 <= 0)
        throw new error("Unable to create socket");

    if(mSocket4 > 0) {
        if (setsockopt(mSocket4, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
            throw new error("Unable to set SO_REUSEADDR");

        hints.ai_family = AF_INET;
        hints.ai_socktype = sockType;
        hints.ai_flags = AI_PASSIVE;
        if(getaddrinfo(NULL, port_s, &hints, &ai ) <0)
            throw new error("Unable to get ipv4 listening address");
        if(::bind(mSocket4, ai->ai_addr, ai->ai_addrlen)<0)
            throw new error("Unable to bind to port");
        freeaddrinfo(ai);
    }

    if(mSocket6 > 0) {
        if (setsockopt(mSocket6, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &yes, sizeof(yes)) < 0)
            throw new error("Unable to set IPV6_V6ONLY");
        if (setsockopt(mSocket6, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
            throw new error("Unable to set SO_REUSEADDR");

        hints.ai_family = AF_INET6;
        hints.ai_socktype = sockType;
        hints.ai_flags = AI_PASSIVE;
        if(getaddrinfo(NULL, port_s, &hints, &ai ) <0)
            throw new error("Unable to get ipv6 listening address");
        if(::bind(mSocket6, ai->ai_addr, ai->ai_addrlen)<0)
            throw new error("Unable to bind to port");
        freeaddrinfo(ai);
    }
}

static inline int max(int a, int b) { return a>b?a:b; }

void *mySocket::_run_tcp(void *ptr)
{
    mySocket *s = (mySocket *) ptr;
    s->run_tcp();

    s->mHandler->clientDisconnected(s);

    delete s;
    return NULL;
}

void mySocket::run_tcp()
{
    if(mConnectedSocket <= 0)
        return;

    if(mHandler)
        if(!mHandler->clientConnected(this))
            return;

    mExit = false;
    while(!mExit) {
        fd_set readfds,writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(mConnectedSocket, &readfds);
        FD_SET(mConnectedSocket, &writefds);
        if (select(mConnectedSocket+1, &readfds, &writefds, NULL, 0) >= 0) {
            if (FD_ISSET(mConnectedSocket, &readfds)) {
                if(!read(mConnectedSocket))
                    return;
            }
            if (FD_ISSET(mConnectedSocket, &writefds)) {
                if(!mHandler->needPacket(this))
                    return;
            }
        }
    }
}

void mySocket::listen()
{
    pthread_t thread;

    if(mConnectedSocket > 0)
        throw new error("Can't listen on connected mySocket");

    if(mTcp) {
        if(mSocket4 > 0) ::listen(mSocket4, 10);
        if(mSocket6 > 0) ::listen(mSocket6, 10);
    }

    mExit = false;
    while(!mExit) {
        fd_set readfds;
        FD_ZERO(&readfds);
        if(mSocket4 > 0) FD_SET(mSocket4, &readfds);
        if(mSocket6 > 0) FD_SET(mSocket6, &readfds);
        if (select(max(mSocket4, mSocket6)+1, &readfds, NULL, NULL, 0) >= 0) {
            if (mSocket4 > 0 && FD_ISSET(mSocket4, &readfds)) {
                if(mTcp) {
                    mEndpointLen = sizeof(mEndpoint);
                    mySocket *s = new mySocket();
                    s->init(::accept(mSocket4, (sockaddr*)&mEndpoint, &mEndpointLen), this);
                    pthread_create(&thread, NULL, _run_tcp, s);
                }
                else
                    read(mSocket4);
            }
            if (mSocket6 > 0 && FD_ISSET(mSocket6, &readfds)) {
                if(mTcp) {
                    mEndpointLen = sizeof(mEndpoint);
                    mySocket *s = new mySocket();
                    s->init(::accept(mSocket6, (sockaddr*)&mEndpoint, &mEndpointLen), this);
                    pthread_create(&thread, NULL, _run_tcp, s);
                }
                else
                    read(mSocket6);
            }
        }
    }
}

mySocket *mySocket::connect(const char *address, int port)
{
    addrinfo *ai, *pai;
    addrinfo hints = {0};
    char port_s[64];
    int sock;

    if(!mTcp)
        throw new error("Can't call connect on a connectionless mySocket");

    snprintf(port_s, sizeof(port_s), "%d", port);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    if(getaddrinfo(address, port_s, &hints, &ai ) <0)
        throw new error("Unable to lookup address");

    pai = ai;
    while(pai) {
        if(pai->ai_family == AF_INET6)
            sock = ::connect(mSocket6, ai->ai_addr, ai->ai_addrlen);
        else if(pai->ai_family == AF_INET)
            sock = ::connect(mSocket4, ai->ai_addr, ai->ai_addrlen);
        else
            sock = -1;

        if(sock > 0) {
            memcpy(&mEndpoint, ai->ai_addr, ai->ai_addrlen);
            mEndpointLen = ai->ai_addrlen;
            mySocket *s = new mySocket();
            s->init(sock, this);
            return s;
        }
        pai = pai->ai_next;
    }
    freeaddrinfo(ai);

    throw new error("Unable to connect to remote host");

//    return NULL;
}

mySocket *mySocket::connect(const sockaddr *to, socklen_t toLen)
{
    int sock;

    if(!mTcp)
        throw new error("Can't call connect on a connectionless mySocket");

    if(to->sa_family == AF_INET6)
        sock = ::connect(mSocket6, to, toLen);
    else if(to->sa_family == AF_INET)
        sock = ::connect(mSocket4, to, toLen);
    else
        sock = -1;

    if(sock > 0) {
        memcpy(&mEndpoint, to, toLen);
        mEndpointLen = toLen;
        mySocket *s = new mySocket();
        s->init(sock, this);
        return s;
    }

    throw new error("Unable to connect to remote host");
//    return NULL;
}

bool mySocket::read(int socket)
{
    sockaddr_storage recvSock;
    socklen_t sl;
    char recvBuf[3000];

    if(mTcp) {
        ssize_t bytes = ::recv(socket, recvBuf, sizeof(recvBuf), 0);
        if((int)bytes <= 0)
            return false;

        recvBuf[bytes] = 0;
        if(mHandler)
            mHandler->packetReceived(this, recvBuf, bytes, (sockaddr *) &mEndpoint, mEndpointLen);
    } else {
        sl = sizeof(recvSock);
        ssize_t bytes = ::recvfrom(socket, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&recvSock, &sl);
        if((int)bytes <= 0)
            return false;

        recvBuf[bytes] = 0;
        if(mHandler)
            mHandler->packetReceived(this, recvBuf, bytes, (sockaddr *) &recvSock, sl);
    }
    return true;
}

ssize_t mySocket::send(const void *packet, size_t packetLen)
{
    char port_s[32];
    int socket4, socket6;
    ssize_t len = -1;

    if(mTcp && mConnectedSocket <= 0)
        throw new error("Can't write to unconnected mySocket");

    if(!mTcp && !mMulticast)
        throw new error("Can't multicast on non-mulicast socket");

    if(mTcp) {
        len = ::send(mConnectedSocket, packet, packetLen, 0);
        if(len < 0)
            throw new error("Unable to send packet");
    }
    else {

        if(!mMulticast)
            throw new error("Can't broadcast on non-multicast socket");

        sprintf(port_s, "%d", mPort);

        /* The packet is sent over both v6 and v4, with v6 taking precedence */

        /* Requires binding to a specific multicast interface IPV6_MULTICAST_IF for OSX */

        if(mSocket6 > 0) {
            socket6 = ::socket(AF_INET6, SOCK_DGRAM, 0);
            if (socket6 < 1)
                throw new error("Unable to create ipv4 mySocket");

            if (setsockopt(socket6, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *) &mInterfaceId,
                           sizeof(mInterfaceId)) < 0)
                throw new error("Unable to set ipv6 multicast if");

            len = ::sendto(socket6, packet, packetLen, 0, (sockaddr *)&mMulticastV6, sizeof(sockaddr_in6));
            if (len < 0)
                throw new error("Unable to send ipv6 packet");
            close(socket6);
        }

        if(mSocket4 > 0) {
            socket4 = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (socket4 < 1)
                throw new error("Unable to create ipv4 mySocket");

            if (::sendto(socket4, packet, packetLen, 0, (sockaddr *)&mMulticastV4, sizeof(sockaddr_in)) < 0)
                throw new error("Unable to send ipv4 packet");
            close(socket4);
        }
    }
    return len;
}

ssize_t mySocket::sendTo(const void *packet, size_t packetLen, const sockaddr *to, socklen_t toLen)
{
    int sock;
    ssize_t len;

    if(mTcp)
        throw new error("Can't call sendTo on a TCP mySocket");

    sock = ::socket(to->sa_family, SOCK_DGRAM, 0);
    len = ::sendto(sock, packet, packetLen, 0, to, toLen);
    close(sock);
    return len;
}

ssize_t mySocket::replyTo(const void *packet, size_t packetLen, const sockaddr *to, socklen_t toLen)
{
    ssize_t len;

    if(mTcp)
        throw new error("Can't call replyTo on a TCP mySocket");

    if(to->sa_family == AF_INET6)
        len = sendto(mSocket6, packet, packetLen, 0, to, toLen);
    else
        len = sendto(mSocket4, packet, packetLen, 0, to, toLen);
    return len;
}

const char *mySocket::interfaceAddress(int af, const char *interfaceName)
{
    struct ifaddrs *myaddrs, *ifa;
    void *in_addr;
    char buf[64] = "";

    mInterfaceId = if_nametoindex(interfaceName);
    if(mInterfaceId<1)
        throw new error("Interface %s not found", interfaceName);

    if(getifaddrs(&myaddrs) != 0)
        throw new error("Unable to get interface address");

    /* Filter out 127.x for v4.  For v6 we do the opposite and only return
       link locals (which windows uses exclusively in WSD) */
    for(ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == NULL) continue;
        if(!(ifa->ifa_flags & IFF_UP)) continue;
        if(strcmp(ifa->ifa_name, interfaceName)) continue;
        if(ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6) continue;
        if(af != ifa->ifa_addr->sa_family) continue;

        switch (ifa->ifa_addr->sa_family)
        {
            case AF_INET:
            {
                struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
                if(ntohl(s4->sin_addr.s_addr)>>24 == 127) continue;
                in_addr = &s4->sin_addr;
                break;
            }

            case AF_INET6:
            {
                struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                if(s6->sin6_addr.s6_addr[0] != 0xfe) continue;
                if(s6->sin6_addr.s6_addr[1] != 0x80) continue;
                in_addr = &s6->sin6_addr;
                break;
            }

            default:
                continue;
        }

        if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf)))
            throw new error("inet_ntop failed");
    }

    freeifaddrs(myaddrs);

    return strdup(buf);
}

const char *mySocket::endpointAddress()
{
    if(mEndpointLen <= 0)
        throw new error("No endpoint");

    if(getnameinfo((sockaddr*)&mEndpoint, mEndpointLen, mEndpointName, sizeof(mEndpointName), NULL, 0, NI_IDN))
        throw new error("Couldn't get endpoint address");
    return mEndpointName;
}

void mySocket::setUserData(void *data)
{
    mUserData = data;
}

void *mySocket::getUserData()
{
    return mUserData;
}
