/**
 * Class definition for rtcMemory
 * 
 * The rtcMemory class provides access to, and methods for manipulation of,
 * the RTC Memory of the ESP8266
 * 
 * Notes:
 * 1) The library routines work in "buckets" of 4 bytes
 * 2) User memory is available from bucket 65 onwards
 * 3) There's supposed to be 128 buckets available, but it seems to fail around 
 *     bucket 184 (i.e. there seems to be 64 buckets available)
 * 4) Advice is, don't let your data cross a bucket boundary at any point
 * 5) Also, deep sleep is OK; power off is not
 * 
 * @author  David Argles, d.argles@gmx.com
 * @version 19dec2020 14:03h
 */

#include "user_interface.h"

#define RTCMEMORYSTART 65 // 

typedef struct{
  int count;
  int thing;
  int errCode;
  int dummy;
} rtcStore;

rtcStore myData;
  
class rtcMemory
{
  private:

  public:

  /* The constructor
  rtcMemory(){
    readData();
    return;
  }*/

  void readData(){
    system_rtc_mem_read(RTCMEMORYSTART, &myData, 8);
    yield();
  }

  void writeData(){
    system_rtc_mem_write(RTCMEMORYSTART, &myData, sizeof(myData));
    yield();
  }

  int count(){
    return myData.count;
  }

  void incrementCount(){
    myData.count++;
    return;
  }
  
  int error(){
    return myData.errCode;
  }

  void setError(int error){
    myData.errCode += error;
    return;
  }

  void setCount(int newValue){
    myData.count = newValue;
    writeData();
    return;
  }
};
