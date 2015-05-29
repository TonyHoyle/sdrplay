#include <mirsdrapi-rsp.h>
#include <stdio.h>

int main(int argc, char **argv)
{
  mir_sdr_ErrT err;
  float ver;
  unsigned sps;

  err = mir_sdr_ApiVersion(&ver);
  if (ver != MIR_SDR_API_VERSION)
  {
    printf("Incorrect API version - have %f want %f", ver, MIR_SDR_API_VERSION);
    return -1;
  }

  err = mir_sdr_Init(40, 2.048, 222.064, mir_sdr_BW_1_536, mir_sdr_IF_Zero, &sps);

  err = mir_sdr_Uninit();
  return 0;
}
