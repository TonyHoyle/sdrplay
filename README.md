# rtl_tcp server for SDRPlay

(C)2015 Tony Hoyle (tony@hoyle.me.uk).  Licensed under the BSD license. 

## NOTES
  The SDRPlay libraries are compiled against a recent libusb - the one that ships with the Raspberry Pi
  is not new enough and will crash.  Suitable upgrades are at https://www.hoyle.me.uk/pi/ 

  The server pretends to be an R820T - this enables gain setting but the values the R820T use don't really map to the SDRPlay, so there won't be the same results.

## BUILDING
```
  mkdir build
  cd build
  cmake ..
  make
```

## HISTORY
  Version 0.0.0: Initial build
  Version 1.0.0: Gain settings should now work in client apps.

## TODO
  AGC is not currently supported, due to lack of documentation.

## BUGS
  The only way to change frequency seems to be to reinit the entire SDRPlay - setRf doesn't work.. this
  makes tuning slow.

  Every now and then the SDRPlay goes loopy in SDR#.  Disconnect and reconnect to fix this.. no idea why this happens.

