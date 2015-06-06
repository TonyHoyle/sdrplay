#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "mySocket.h"
#include "error.h"
#include "sdrplay.h"

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

#define RTLSDR_TUNER_R820T 5
// We don't actually use the R820T gain list.. for a start they go up to 1/10db and we don't support that,
// but ours go in the other direction too (they're more losses than gains.. the docs really don't explain why).
const int gain_list[] = { 84, 81, 78, 75, 72, 69, 66, 63, 60, 57, 54, 51, 48, 45, 42, 39, 36, 33, 30, 27, 24, 21, 18, 15, 12, 9,
                            6, 3, 0 };

class sdrServer : socketEvent
{
private:
    bool mDebug;
    bool mIpv4, mIpv6;
    int mPort;
    short *mI, *mQ;
    uint8_t *mS;
    double mFrequency, mSampleRate;
    int mGain;
    mySocket mSocket;
    SDRPlay mSdrPlay;
    uint8_t mPartialCommand[sizeof(sdr_command)];
    uint8_t *mPartialCommandPtr;

    void processSdrCommand(sdr_command *sdrcmd);
    double intToDouble(int freq);

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
    int frequency = 14200000;
    int port = 1234;
    int samplerate = 2048000;
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
    mFrequency = intToDouble(frequency);
    mSampleRate = intToDouble(samplerate);
    mGain = 70;
    mPort = port;
    mIpv4 = ipv4;
    mIpv6 = ipv6;
    mDebug = debug;
    mPartialCommandPtr = mPartialCommand;
    mI = NULL;
    mQ = NULL;
    mS = NULL;

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
    delete[] mI;
    delete[] mQ;
    delete[] mS;
}

int sdrServer::run()
{
    try {
        printf("mSampleRate = %f\n", mSampleRate);
        printf("mFrequency = %f\n", mFrequency);
        mSdrPlay.init(mGain, mSampleRate, mFrequency, mir_sdr_BW_1_536, mir_sdr_IF_Zero);

        mSdrPlay.setDCMode(dcmOneShot, false);
        mSdrPlay.setDCTrackTime(63);

        mI = new short[mSdrPlay.getSamplesPerPacket()];
        mQ = new short[mSdrPlay.getSamplesPerPacket()];
        mS = new uint8_t[mSdrPlay.getSamplesPerPacket() * 2];

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
    /* We pretend to be and R820 as that seems to be the one with the most settings.  The protocol
     * doesn't allow for arbitrary devices */
    sdr_info info = { {'R', 'T', 'L', '0' } };

    info.tuner_type = htonl(RTLSDR_TUNER_R820T);
    info.tuner_gain_count = htonl(sizeof(gain_list)/sizeof(gain_list[0]));

    if(mDebug)
        printf("Connection made from %s\n", socket->endpointAddress());

    if(socket->send(&info, sizeof(info)) <= 0) {
        if(mDebug)
            printf("Sending initial packet failed\n");
        return false;
    }

   return true;
}

void sdrServer::clientDisconnected(mySocket *)
{
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
    short *i, *q;
    uint8_t *s;
    int count = mSdrPlay.getSamplesPerPacket();

    try {
        if(mSdrPlay.readPacket(mI, mQ, NULL, NULL, NULL) == 0) {
            for(i=mI, q=mQ, s=mS; i<mI+count; i++, q++) {
                *(s++) = (uint8_t)(*i>>8)+128;
                *(s++) = (uint8_t)(*q>>8)+128;
            }
            socket->send(mS, (size_t) count * 2);
        }
    }
    catch(error *e) {
        printf("%s\n", e->text());
        return false;
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
            mFrequency = intToDouble(arg);
            if(mDebug) printf("Set frequency in Mhz to %f\n", mFrequency);
            // in Mhz
            mSdrPlay.uninit();
            mSdrPlay.init(mGain, mSampleRate, mFrequency, mir_sdr_BW_1_536, mir_sdr_IF_Zero);
//            mSdrPlay.setRF(mFrequency, false, false);
            break;
        case 2: // Set Sample Rate
            mSampleRate = intToDouble(arg);
            if(mDebug) printf("Set sample rate in Hz to %f\n", mSampleRate);
            // in Hz
//            mSdrPlay.uninit();
//            mSdrPlay.init(mGain, mSampleRate, mFrequency, mir_sdr_BW_1_536, mir_sdr_IF_Zero);
            mSdrPlay.setFS(mSampleRate, false, false, false);
            break;
        case 3: // Set Gain Mode
            if(mDebug) printf("Set gain mode to %d\n", arg);
            break;
        case 4: // Set Tuner Gain
            mGain = int(arg / 10.0 + 0.5);
            if(mDebug) printf("Set tuner gain to %d\n", mGain);
            mSdrPlay.setGR(mGain, true, false);
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
            mGain = gain_list[arg];
            if(mDebug) printf("             %d\n", mGain);
            mSdrPlay.setGR(mGain, true, false);
            break;
        default:
            if(mDebug) printf("Unknown Cmd = %d, arg = %d\n", cmd, arg );
            break;
    }
}

double sdrServer::intToDouble(int freq)
{
    return double(freq) / 1000000.0;
}
