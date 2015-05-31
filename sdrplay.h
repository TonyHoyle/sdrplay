// SDRPlay controller object
#ifndef SDRPLAY__H
#define SDRPLAY__H

#include <mirsdrapi-rsp.h>

class SDRPacketQueue
{
protected:
	short *mI;
	short *mQ;
	int mFs;
	int mSamplesPerPacket;
	int mPacketCount;
	int mReadPoint;
	int mWritePoint;
	
public:
	SDRPacketQueue(int samplesPerPacket, int packetCount, bool i, bool q);
	virtual ~SDRPacketQueue();
	
	bool getPacket(bool forWrite, short **I, short **Q);
	bool hasData();

    int getPacketSize();
};

enum DCMode_t
{
	dcmStatic  = 0,
	dcmOneShot = 4,
	dcmContinuous = 5
};

class SDRPlay 
{
protected:
  int mSamplesPerPacket;
  
public:
  SDRPlay();
  virtual ~SDRPlay();
  
  mir_sdr_ErrT init(int gainReduction, double adcFreq, double rfFreq, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType);
  mir_sdr_ErrT uninit();
  mir_sdr_ErrT setRF(double freq, bool absolute, bool syncUpdate);
  mir_sdr_ErrT readPacket(SDRPacketQueue *packet, bool* grChanged, bool* rfChanged, bool* fsChanged);
  mir_sdr_ErrT setFS(double freq, bool absolute, bool syncUpdate, bool reCal);
  mir_sdr_ErrT setGR(int gainReduction, bool absolute, bool syncUpdate);
  mir_sdr_ErrT setGRParams(int minimumGr, int lnaGrThreshold);
  mir_sdr_ErrT setDCMode(DCMode_t mode, bool speedUp);
  mir_sdr_ErrT setDCTrackTime(int trackTime);
  mir_sdr_ErrT setSyncUpdateSampleNum(unsigned sampleNum);
  mir_sdr_ErrT setSyncUpdatePeriod(unsigned period);
  float apiVersion();
  mir_sdr_ErrT resetUpdateFlags(bool resetGainUpdate, bool resetRfUpdate, bool resetFsUpdate);
  
  SDRPacketQueue *newPacketQueue(int packetCount);
};

#endif