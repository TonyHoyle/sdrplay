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
#include <stdlib.h>

#include "error.h"
#include "mySocket.h"

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
//#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

#ifndef NI_IDN
#define NI_IDN 0
#endif

// TODO: Given an interface name, determine the ipv4 and ipv6 link local addressses, and interface number.

mySocket::mySocket(socketEvent *handler, const char *interface, bool tcp)
{
    mHandler = handler;
    mInterfaceName = strdup(interface);
    mSocket4 = -1;
    mSocket6 = -1;
    mTcp = tcp;
    mConnectedSocket = -1;
    mEndpointLen = 0;
    lookupInterface();
}

mySocket::mySocket(int connectedSocket, mySocket *parentSocket)
{
    mTcp = true;
    mConnectedSocket = connectedSocket;
    mHandler = parentSocket->mHandler;
    mInterfaceName = parentSocket->mInterfaceName;
    mEndpoint = parentSocket->mEndpoint;
    mEndpointLen = parentSocket->mEndpointLen;
    mSocket4 = -1;
    mSocket6 = -1;
}

mySocket::~mySocket()
{
    if(mSocket4 > 0)
        close(mSocket4);
    if(mSocket6 > 0)
        close(mSocket6);
    if(mConnectedSocket > 0)
        close(mConnectedSocket);

    free((void*)mInterface4);
    free((void*)mInterface6);
    free((void*)mInterfaceName);
    free((void*)mAddress);

}

void mySocket::bind(const char *address, int port, bool ipv4, bool ipv6, bool multicast)
{
    int yes = 1;
    int no = 0;
    ip_mreq mreq;
    ipv6_mreq mreq6;
    addrinfo *ai;
    addrinfo hints = {0};
    char port_s[32];
    int sockType;

    if(mConnectedSocket > 0)
        throw new error("Can't bind on connected mySocket");

    sprintf(port_s,"%d", port);
    mAddress = strdup(address);
    mPort = port;
    mMulticast = multicast;

    if(mTcp) sockType = SOCK_STREAM;
    else sockType = SOCK_DGRAM;

    if(ipv4) {
        mSocket4 = ::socket(AF_INET, sockType, 0);
        if (mSocket4 < 1)
            throw new error("Unable to create ipv4 mySocket");

        if(setsockopt(mSocket4, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))<0)
            throw new error("Unable to set SO_REUSEADDR");
        if(setsockopt(mSocket6, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))<0)
            throw new error("Unable to set SO_REUSEADDR");

        if(multicast) {
            if (setsockopt(mSocket4, IPPROTO_IP, IP_MULTICAST_LOOP, &no, sizeof(no)) < 0)
                throw new error("Unable to set IP_MULTICAST_LOOP");
        }
    }

    if(ipv6) {
        mSocket6 = ::socket(AF_INET6, sockType, 0);
        if (mSocket6 < 1)
            throw new error("Unable to create ipv6 mySocket");

        if (setsockopt(mSocket6, IPPROTO_IPV6, IPV6_V6ONLY, (void *) &yes, sizeof(yes)) < 0)
            throw new error("Unable to set IPV6_V6ONLY");

        if(multicast) {
            if (setsockopt(mSocket6, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &no, sizeof(no)) < 0)
                throw new error("Unable to set IPV6_MULTICAST_LOOP");
        }
    }

    if(mSocket4 > 0) {
        hints.ai_family = AF_INET;
        hints.ai_socktype = sockType;
        hints.ai_flags = AI_PASSIVE;
        if(getaddrinfo(NULL, port_s, &hints, &ai ) <0)
            throw new error("Unable to get ipv4 listening address");
        if(::bind(mSocket4, ai->ai_addr, ai->ai_addrlen)<0)
            throw new error("Unable to bind to port");
        freeaddrinfo(ai);

        if(multicast) {
            mreq.imr_multiaddr.s_addr = inet_addr(address);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            if (setsockopt(mSocket4, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
                throw new error("ipv4 group membership failed");
        }
    }

    if(mSocket6 > 0) {
        hints.ai_family = AF_INET6;
        hints.ai_socktype = sockType;
        hints.ai_flags = AI_PASSIVE;
        if(getaddrinfo(NULL, port_s, &hints, &ai ) <0)
            throw new error("Unable to get ipv6 listening address");
        if(::bind(mSocket6, ai->ai_addr, ai->ai_addrlen)<0)
            throw new error("Unable to bind to port");
        freeaddrinfo(ai);

        if(multicast) {
            if (!inet_pton(AF_INET6, address, &mreq6.ipv6mr_multiaddr))
                throw new error("Unable to parse v6 host");

            mreq6.ipv6mr_interface = 0;

            if (setsockopt(mSocket6, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, sizeof(mreq6)) < 0)
                throw new error("ipv6 group membership failed");
        }
    }
}

static inline int max(int a, int b) { return a>b?a:b; }

void mySocket::listen()
{
    if(mConnectedSocket > 0)
        throw new error("Can't listen on connected mySocket");

    mExit = false;
    while(!mExit) {
        fd_set socks;
        FD_ZERO(&socks);
        if(mSocket4 > 0) FD_SET(mSocket4, &socks);
        if(mSocket6 > 0) FD_SET(mSocket6, &socks);
        if(mConnectedSocket > 0) FD_SET(mSocket6, &socks);
        if (select(max(max(mSocket4, mSocket6), mConnectedSocket), &socks, NULL, NULL, 0) >= 0) {
            if (mSocket4 > 0 && FD_ISSET(mSocket4, &socks)) {
                if(mTcp) {
                    mEndpointLen = sizeof(mEndpoint);
                    new mySocket(::accept(mSocket4, (sockaddr*)&mEndpoint, &mEndpointLen), this);
                }
                else read(mSocket4);
            }
            if (mSocket6 > 0 && FD_ISSET(mSocket6, &socks)) {
                if(mTcp) {
                    mEndpointLen = sizeof(mEndpoint);
                    new mySocket(::accept(mSocket6, (sockaddr*)&mEndpoint, &mEndpointLen), this);
                }
                else read(mSocket6);
            }
            if (mConnectedSocket > 0 && FD_ISSET(mConnectedSocket, &socks)) {
                read(mConnectedSocket);
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
            return new mySocket(sock, this);
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
        return new mySocket(sock, this);
    }

    throw new error("Unable to connect to remote host");
//    return NULL;
}

void mySocket::read(int socket)
{
    sockaddr_storage recvSock;
    socklen_t sl;
    char recvBuf[3000];

    if(mTcp) {
        ssize_t bytes = ::read(socket, recvBuf, 0);
        recvBuf[bytes] = 0;
        mHandler->packetReceived(recvBuf, bytes, (sockaddr *) &mEndpoint, mEndpointLen);
    } else {
        sl = sizeof(recvSock);
        ssize_t bytes = ::recvfrom(socket, recvBuf, sizeof(recvBuf), 0, (sockaddr*)&recvSock, &sl);
        recvBuf[bytes] = 0;
        mHandler->packetReceived(recvBuf, bytes, (sockaddr *) &recvSock, sl);
    }
}

ssize_t mySocket::send(const char *packet, size_t packetLen)
{
    addrinfo hints = {0};
    char port_s[32];
    addrinfo *ai;
    int socket4, socket6;
    ssize_t len = -1;

    if(mTcp && mConnectedSocket <= 0)
        throw new error("Can't write to unconnected mySocket");

    if(mTcp) {
        len = ::send(mConnectedSocket, packet, packetLen, 0);
        if(len < 0)
            throw new error("Unable to send packet");
    }
    else {
        sprintf(port_s, "%d", mPort);

        /* The packet is sent over both v6 and v4, with v6 taking precedence */

        /* Requires binding to a specific multicast interface IPV6_MULTICAST_IF for OSX */

        if(mSocket6 > 0) {
            socket6 = ::socket(AF_INET6, SOCK_DGRAM, 0);
            if (socket6 < 1)
                throw new error("Unable to create ipv4 mySocket");

            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_DGRAM;
            if (getaddrinfo(mAddress, port_s, &hints, &ai) < 0)
                throw new error("Unable to get ipv6 sending address");
            if(mMulticast) {
                if (setsockopt(socket6, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const char *) &mInterfaceId,
                               sizeof(mInterfaceId)) <
                    0)
                    throw new error("Unable to set ipv6 multicast if");
            }
            len = ::sendto(socket6, packet, packetLen, 0, ai->ai_addr, ai->ai_addrlen);
            if (len < 0)
                throw new error("Unable to send ipv6 packet");
            freeaddrinfo(ai);
            close(socket6);
        }

        if(mSocket4 > 0) {
            socket4 = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (socket4 < 1)
                throw new error("Unable to create ipv4 mySocket");

            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            if (getaddrinfo(mAddress, port_s, &hints, &ai) < 0)
                throw new error("Unable to get ipv4 sending address");
            if (::sendto(socket4, packet, packetLen, 0, ai->ai_addr, ai->ai_addrlen) < 0)
                throw new error("Unable to send ipv4 packet");
            freeaddrinfo(ai);
            close(socket4);
        }
    }
    return len;
}

ssize_t mySocket::sendTo(const char *packet, size_t packetLen, const sockaddr *to, socklen_t toLen)
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

ssize_t mySocket::replyTo(const char *packet, size_t packetLen, const sockaddr *to, socklen_t toLen)
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

void mySocket::lookupInterface()
{
    struct ifaddrs *myaddrs, *ifa;
    void *in_addr;
    char buf[64];

    mInterfaceId = if_nametoindex(mInterfaceName);
    if(mInterfaceId<1)
        throw new error("Interface %s not found", mInterfaceName);

    if(getifaddrs(&myaddrs) != 0)
        throw new error("Unable to get interface address");

    /* Filter out 127.x for v4.  For v6 we do the opposite and only return
       link locals (which windows uses exclusively in WSD) */
    for(ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr == NULL) continue;
        if(!(ifa->ifa_flags & IFF_UP)) continue;
        if(strcmp(ifa->ifa_name, mInterfaceName)) continue;
        if(ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6) continue;

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

        switch (ifa->ifa_addr->sa_family)
        {
            case AF_INET:
                mInterface4 = strdup(buf);
                break;

            case AF_INET6:
                mInterface6 = strdup(buf);

            default:
                continue;
        }
    }

    freeifaddrs(myaddrs);
}

const char *mySocket::interfaceAddress(int af)
{
    if(af == AF_INET6)
        return mInterface6;
    else
        return mInterface4;
}

const char *mySocket::endpointAddress()
{
    if(mEndpointLen <- 0)
        throw new error("No endpoint");

    if(getnameinfo((sockaddr*)&mEndpoint, mEndpointLen, mEndpointName, sizeof(mEndpointName), NULL, 0, NI_IDN))
        throw new error("Couldn't get endpoint address");
    return mEndpointName;
}

