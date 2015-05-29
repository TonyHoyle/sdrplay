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
	
	short *getI(bool write);
	short *getQ(bool write);
	bool hasData();
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
  mir_sdr_errT uninit();
  mir_sdr_errT setRF(double freq, bool absolute, bool syncUpdate);
  mir_sdr_errT readPacket(SDRPacketQueue *packet, bool& grChanged, rfChanged, fsChanged);
  mir_sdr_errT setFS(double freq, bool absolute, bool syncUpdate);
  mir_sdr_errT setGR(int gainReduction, bool absolute, bool syncUpdate);
  mir_sdr_errT setGRParams(int minimumGr, int lnaGrThreshold);
  mir_sdr_errT setDCMode(DCMode_t mode, bool speedUp);
  mir_sdr_errT setDCTrackTime(int trackTime);
  mir_sdr_errT setSyncUpdateSampleNum(unsigned sampleNum);
  mir_sdr_errT setSyncUpdatePeriod(unsigned period);
  float apiVersion();
  mir_Sdr_errT resetUpdateFlags(bool resetGainUpdate, bool resetRfUpdate, bool resetFsUpdate);
  
  SDRPacket *newPacket(int count);
};

#endif