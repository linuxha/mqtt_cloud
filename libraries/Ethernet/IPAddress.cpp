// https://projects.cs.uaf.edu/redmine/projects/cyberalaska/repository/revisions/master/entry/include/arduino/hardware/cores/robot/IPAddress.cpp

//#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

//#include <IPAddress.h>
#include "IPAddress.h"

IPAddress::IPAddress()
{
    //memset(_address, 0, sizeof(_address));
    //IPAddress(0);
    IPAddress((uint32_t) 0);
}

IPAddress::IPAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet)
{
    _address[0] = first_octet;
    _address[1] = second_octet;
    _address[2] = third_octet;
    _address[3] = fourth_octet;
}

IPAddress::IPAddress(uint32_t address)
{
    memcpy(_address, &address, sizeof(_address));
}

IPAddress::IPAddress(const uint8_t *address)
{
    memcpy(_address, address, sizeof(_address));
}

IPAddress& IPAddress::operator=(const uint8_t *address)
{
    memcpy(_address, address, sizeof(_address));
    return *this;
}

IPAddress& IPAddress::operator=(uint32_t address)
{
    memcpy(_address, (const uint8_t *)&address, sizeof(_address));
    return *this;
}

bool IPAddress::operator==(const uint8_t* addr)
{
    return memcmp(addr, _address, sizeof(_address)) == 0;
}

size_t IPAddress::printTo(Print& p) const
{
    size_t n = 0;
    for (int i =0; i < 3; i++)
    {
        p.print(_address[i], DEC);
        p.print('.');
    }
    p.print(_address[3], DEC);

    return n;
}
