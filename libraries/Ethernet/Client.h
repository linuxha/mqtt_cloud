#ifndef client_h
#define client_h

#include "Print.h"
#include "Stream.h"
#include "IPAddress.h"

class Client : public Stream {
public:
  virtual int connect(IPAddress ip, uint16_t port) = 0;
  virtual int connect(const char *host, uint16_t port) = 0;

#if defined(__PIC32MX__) && (ARDUINO < 100)
  virtual void write(uint8_t) = 0;
  virtual void write(const uint8_t *buffer, size_t size) = 0;
#else
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buffer, size_t size);
#endif

  virtual int      available() = 0;
  virtual int      read() = 0;
  virtual int      read(uint8_t *buf, size_t size) = 0;
  virtual int      peek() = 0;
  virtual void     flush() = 0;
  virtual void     stop() = 0;
  virtual uint8_t  connected() = 0;
  virtual operator bool() = 0;

  /*
    class Accel {
      public:
        virtual void initialize(void) = 0;        //either pure virtual
	virtual void measure(void) = 0; 
	virtual void calibrate(void) {};          //or implementation 
	virtual const int getFlightData(byte) {};
	};
  */
protected:
  uint8_t* rawIPAddress(IPAddress& addr) { return addr.raw_address(); };
};

#endif
