// SDRPlay controller object
#include "sdrplay.h"

#define null ((void*)0)  

SDRPlay::SDRPlay()
{
	mSamplesPerPacket = 0;
}

SDRPlay::~SDRPlay()
{
	uninit();
}

 mir_sdr_ErrT SDRPlay::init(int gainReduction, double adcFreq, double rfFreq, mir_sdr_Bw_MHzT bwType, mir_sdr_If_kHzT ifType)
 {
	 return mir_sdr_Init(gainReduction, adcFreq, rfFreq, bwType, ifType, &mSamplesPerPacket);
 }
 
 mir_sdr_ErrT SDRPlay::uninit()
 {
	 return mir_sdr_Uninit();
 }
 
mir_sdr_ErrT SDRPlay::setRF(double freq, bool absolute, bool syncUpdate)
{
	return mir_sdr_SetRf(freq, absolute?1:0, syncUpdate?1:0);
}

mir_sdr_ErrT SDRPlay::readPacket(SDRPacketQueue *packet, bool& grChanged, bool& rfChanged, bool& fsChanged)
{
}
 
mir_sdr_ErrT setFS(double freq, bool absolute, bool syncUpdate)
{
}
  
mir_sdr_ErrT setGR(int gainReduction, bool absolute, bool syncUpdate)
{
}

mir_sdr_ErrT setGRParams(int minimumGr, int lnaGrThreshold)
{
}

mir_sdr_ErrT setDCMode(DCMode_t mode, bool speedUp)
{
}

mir_sdr_ErrT setDCTrackTime(int trackTime)
{
}

mir_sdr_ErrT setSyncUpdateSampleNum(unsigned sampleNum)
{
}

mir_sdr_ErrT setSyncUpdatePeriod(unsigned period)
{
}

float apiVersion()
{
}

mir_sdr_ErrT resetUpdateFlags(bool resetGainUpdate, bool resetRfUpdate, bool resetFsUpdate)
{
}
  
SDRPacketQueue *SDRPlay::newPacketQueue(int packetCount)
{
	return new SDRPacketQueue(mSamplesPerPacket, packetCount, false, true);
}

// SDRPacketQueue

SDRPacketQueue::SDRPacketQueue(int samplesPerPacket, int packetCount, bool i, bool q)
{
	int size = samplesPerPacket * packetCount;
	
	if(i) mI = new short[size];
	else mI = (short*)null;
	
	if(q) mQ = new short[size];
	else mQ = (short*)null;
	
	mSamplesPerPacket = samplesPerPacket;
	mPacketCount = packetCount;
	mReadPoint = 0;
	mWritePoint = 0;
}

SDRPacketQueue::~SDRPacketQueue()
{
	if(mI) delete[](mI);
	if(mQ) delete[](mQ);
}
	
bool SDRPacketQueue::getPacket(bool forWrite, short **I, short **Q)
{
	if(forWrite) {
		if(!mI) (*I) = (short*)null;
		else (*I) = mI+mSamplesPerPacket+mWritePoint;
		if(!mQ) (*Q) = (short*)null;
		else (*Q) = mQ+mSamplesPerPacket+mWritePoint;
		if(++mWritePoint = mPacketCount) mWritePoint = 0;
	} else {
		if(!hasData()) return false;
		
		if(!mI) (*I) = (short*)null;
		else (*I) = mI+mSamplesPerPacket+mReadPoint;
		if(!mQ) (*Q) = (short*)null;
		else (*Q) = mQ+mSamplesPerPacket+mReadPoint;
		if(++mReadPoint = mPacketCount) mReadPoint = 0;
	}
	return true;
}

bool SDRPacketQueue::hasData()
{
	return mReadPoint!=mWritePoint;
}
