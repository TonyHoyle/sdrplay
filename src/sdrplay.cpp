// SDRPlay controller object
#include <stdio.h>
#include "sdrplay.h"

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

mir_sdr_ErrT SDRPlay::readPacket(short *I, short *Q, bool* grChanged, bool* rfChanged, bool* fsChanged)
{
	int _grChanged, _rfChanged, _fsChanged;
	unsigned firstSampleNum;
    mir_sdr_ErrT res;

	res = mir_sdr_ReadPacket(I, Q, &firstSampleNum, &_grChanged, &_rfChanged, &_fsChanged);
    if(grChanged) *grChanged = _grChanged!=0;
    if(rfChanged) *rfChanged = _rfChanged!=0;
    if(fsChanged) *fsChanged = _fsChanged!=0;
//	mir_sdr_SetSyncUpdateSampleNum(firstSampleNum);
    return res;
}
 
mir_sdr_ErrT SDRPlay::setFS(double freq, bool absolute, bool syncUpdate, bool reCal)
{
	return mir_sdr_SetFs(freq, absolute?1:0, syncUpdate?1:0, reCal?1:0);
}
  
mir_sdr_ErrT SDRPlay::setGR(int gainReduction, bool absolute, bool syncUpdate)
{
	return mir_sdr_SetGr(gainReduction, absolute?1:0, syncUpdate?1:0);
}

mir_sdr_ErrT SDRPlay::setGRParams(int minimumGr, int lnaGrThreshold)
{
	return mir_sdr_SetGrParams(minimumGr, lnaGrThreshold);
}

mir_sdr_ErrT SDRPlay::setDCMode(DCMode_t mode, bool speedUp)
{
	return mir_sdr_SetDcMode((int)mode, speedUp?1:0);
}

mir_sdr_ErrT SDRPlay::setDCTrackTime(int trackTime)
{
	return mir_sdr_SetDcTrackTime(trackTime);
}

mir_sdr_ErrT SDRPlay::setSyncUpdateSampleNum(unsigned sampleNum)
{
	return mir_sdr_SetSyncUpdateSampleNum(sampleNum);
}

mir_sdr_ErrT SDRPlay::setSyncUpdatePeriod(unsigned period)
{
	return mir_sdr_SetSyncUpdatePeriod(period);
}

float SDRPlay::apiVersion()
{
	float v;
	mir_sdr_ApiVersion(&v);
	return v;
}

mir_sdr_ErrT SDRPlay::resetUpdateFlags(bool resetGainUpdate, bool resetRfUpdate, bool resetFsUpdate)
{
	return mir_sdr_ResetUpdateFlags(resetGainUpdate?1:0, resetRfUpdate?1:0, resetFsUpdate?1:0);
}

int SDRPlay::getSamplesPerPacket()
{
    return mSamplesPerPacket;
}
