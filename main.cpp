#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "mySocket.h"
#include "error.h"
#include "sdrplay.h"

static const int QUEUE_SIZE = 25;

struct sdr_info
{
    char magic[4];
    int32_t tuner_type;
    int32_t tuner_gain_count;
} __attribute__((packed));

struct sdr_command
{
    int8_t cmd;
    int32_t param;
} __attribute__((packed));

class sdrServer : socketEvent
{
private:
    bool mDebug;
    pthread_t mThread;
    bool mIpv4, mIpv6;
    int mPort;
    int mFrequency, mSampleRate;
    int mGain;
    mySocket mSocket;
    SDRPlay mSdrPlay;
    uint8_t mPartialCommand[sizeof(sdr_command)];
    uint8_t *mPartialCommandPtr;

    void processSdrCommand(sdr_command *sdrcmd);
    double intToDouble(int freq);

    struct sdrData
    {
        sdrServer *server;
        mySocket *socket;
    };
    static void *_sdrserver_send(void *data);
    void sdrserver_send(mySocket *socket);

public:
    sdrServer(int frequency, int port, int samplerate, bool ipv4, bool ipv6, bool debug);
    virtual ~sdrServer();

    int run();

    virtual bool clientConnected(mySocket *socket);
    virtual void clientDisconnected(mySocket *socket);
    virtual void packetReceived(mySocket *socket, const void *packet, ssize_t packetLen, sockaddr *, socklen_t);
    virtual bool needPacket(mySocket *socket);
};

void usage()
{
    printf("usage: sdr_tcp [-f frequency][-p port][-s samplerate][-4][-6][-d]\n");
}

int main(int argc, char **argv)
{
    int c;
    int frequency = 1420000;
    int port = 1234;
    int samplerate = 204800;
    bool ipv4 = true;
    bool ipv6 = true;
    bool debug = false;

    while((c = getopt(argc, argv, "f:p:s:46d")) > 0)
        switch(c)
        {
            case 'f':
                frequency = atoi(optarg);
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 's':
                samplerate = atoi(optarg);
                break;
            case '4':
                ipv6 = false;
                break;
            case '6':
                ipv4 = false;
                break;
            case 'd':
                debug = true;
                break;
            default:
                usage();
                return -1;
        }

    sdrServer server(frequency, port, samplerate, ipv4, ipv6, debug);
    return server.run();
}

/* sdrServer */

sdrServer::sdrServer(int frequency, int port, int samplerate, bool ipv4, bool ipv6, bool debug)
{
    mFrequency = frequency;
    mSampleRate = samplerate;
    mGain = 40;
    mPort = port;
    mIpv4 = ipv4;
    mIpv6 = ipv6;
    mDebug = debug;
    mPartialCommandPtr = mPartialCommand;

    try {
        mSocket.init(this, true);
    }
    catch(error *e) {
        fprintf(stderr,"%s\n", e->text());
        exit(-1);
    }
}

sdrServer::~sdrServer()
{
}

int sdrServer::run()
{
    try {
        mSdrPlay.init(mGain, intToDouble(mSampleRate), intToDouble(mFrequency), mir_sdr_BW_1_536, mir_sdr_IF_Zero);
        mSdrPlay.setDCMode(dcmOneShot, false);
        mSdrPlay.setDCTrackTime(63);

        if(mDebug)
            printf("SDR server listening on port %d\n", mPort);

        mSocket.bind(mPort, mIpv4, mIpv6);
        mSocket.listen();
    }
    catch(error *e) {
        fprintf(stderr,"%s\n", e->text());
        exit(-1);
    }
    return 0;
}

bool sdrServer::clientConnected(mySocket *socket)
{
    sdr_info info = { {'R', 'T', 'L', '\0' }, 0, 64 };

    if(mDebug)
        printf("Connection made from %s\n", socket->endpointAddress());
    if(socket->send(&info, sizeof(info)) <= 0)
        return false;

    sdrData *data = new sdrData;
    data->server = this;
    data->socket = socket;
    pthread_create(&mThread, NULL, _sdrserver_send, &data);
    return true;
}

void sdrServer::clientDisconnected(mySocket *socket)
{
    pthread_cancel(mThread);
}

void *sdrServer::_sdrserver_send(void *data)
{
    sdrServer *This = ((sdrData *)data)->server;
    mySocket *socket = ((sdrData *)data)->socket;

    delete (sdrData *)data;
    This->sdrserver_send(socket);
    return NULL;
}

void sdrServer::sdrserver_send(mySocket *socket)
{
    SDRPacketQueue *queue = mSdrPlay.newPacketQueue(QUEUE_SIZE);

    if(mDebug)
        printf("Packet collection thread started\n");
    socket->setUserData(queue);
    while(mSdrPlay.readPacket(queue, NULL, NULL, NULL) == 0)
        ;
    delete queue;
}

void sdrServer::packetReceived(mySocket *socket, const void *packet, ssize_t packetLen, sockaddr *, socklen_t)
{
    sdr_command *s;

    if(mDebug)
        printf("Received %d byte packet from %s\n", (int)packetLen, socket->endpointAddress());

    if(mPartialCommandPtr > mPartialCommand) {
        ssize_t l = mPartialCommandPtr - mPartialCommand;
        ssize_t r = sizeof(sdr_command) - l;

        if(packetLen >= r) {
            memcpy(mPartialCommandPtr, packet, (size_t)r);
            packetLen -= r;
            packet = ((uint8_t *)packet) + r;
            processSdrCommand((sdr_command *)mPartialCommand);
            mPartialCommandPtr = mPartialCommand;
        } else {
            memcpy(mPartialCommandPtr, packet, (size_t)packetLen);
            mPartialCommandPtr += (size_t)packetLen;
            return;
        }
    }

    s = (sdr_command *)packet;
    while((size_t)packetLen >= sizeof(sdr_command)) {
        processSdrCommand(s++);
        packetLen -= sizeof(sdr_command);
    }

    if(packetLen != 0) {
        memcpy(mPartialCommand, s, (size_t)packetLen);
        mPartialCommandPtr = mPartialCommand + packetLen;
    }
}

bool sdrServer::needPacket(mySocket *socket)
{
    SDRPacketQueue *queue = (SDRPacketQueue *)socket->getUserData();
    short *I, *Q;

    if(queue->hasData()) {
        queue->getPacket(false, &I, &Q);
        socket->send(Q, (size_t) queue->getPacketSize());
    }

    return true;
}

void sdrServer::processSdrCommand(sdr_command *sdrcmd)
{
    int cmd = sdrcmd->cmd;
    int arg = htonl(sdrcmd->param);

    switch(cmd)
    {
        case 1: // Set Frequency
            if(mDebug) printf("Set frequency to %f\n", intToDouble(arg));
            mFrequency = arg;
            mSdrPlay.setRF(intToDouble(arg), false, false);
            break;
        case 2: // Set Sample Rate
            if(mDebug) printf("Set sample rate to %f\n", intToDouble(arg));
            mSampleRate = arg;
            mSdrPlay.setFS(intToDouble(arg), false, false, false);
            break;
        case 3: // Set Gain Mode
            if(mDebug) printf("Set gain mode to %d\n", arg);
            break;
        case 4: // Set Tuner Gain
            if(mDebug) printf("Set tuner gain to %d\n", arg);
            mGain = arg;
            mSdrPlay.setGR(arg, true, false);
            break;
        case 5: // Set Freq Correction
            if(mDebug) printf("Set frequency correction %f\n", intToDouble(arg));
            break;
        case 6: //  Set IF Gain
            if(mDebug) printf("Set if gain to %d\n", arg);
            break;
        case 7: // Set test mode
            if(mDebug) printf("Set test mode to %d\n", arg);
            break;
        case 8: // Set AGC mode
            if(mDebug) printf("Set agc mode to %d\n", arg);
            break;
        case 9: // Set direct sampling // Sample directly off IF or tuner
            if(mDebug) printf("Set direct sampling to %d\n", arg);
            break;
        case 10: // Set offset tuning // Essentially whether to use IF stage or not
            if(mDebug) printf("Set offset tuning to %d\n", arg);
            break;
        case 11: // Set rtl xtal
            if(mDebug) printf("Set rtl xtal to %d\n", arg);
            break;
        case 12: // Set tuner xtal
            if(mDebug) printf("Set tuner xtal to %d\n", arg);
            break;
        case 13: // Set gain by index
            if(mDebug) printf("Set gain to index %d\n", arg);
            break;
        default:
            if(mDebug) printf("Unknown Cmd = %d, arg = %d\n", cmd, arg );
            break;
    }
}

double sdrServer::intToDouble(int freq)
{
    return double(freq) / 100000.0;
}
