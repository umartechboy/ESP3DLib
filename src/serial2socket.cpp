/*
  serial2socket.cpp -  serial 2 socket functions class

  Copyright (c) 2014 Luc Lebosse. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "esp3dlibconfig.h"

#if defined(ESP3D_WIFISUPPORT)
#include "serial2socket.h"
#include "wificonfig.h"
#include <WebSocketsServer.h>
#include <WiFi.h>
Serial_2_Socket Serial2Socket;


void (*echoFunc)(const char*) = 0;
void echo(const char* _echo){
    if (echoFunc)
        echoFunc(_echo);
}
void Serial_2_Socket::SetEcho(void (*_echo)(const char*)){
    echoFunc = _echo;
}
Serial_2_Socket::Serial_2_Socket()
{
    // echo("Serial2Socket::Serial_2_Socket()");
    _web_socket = NULL;
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
}
Serial_2_Socket::~Serial_2_Socket()
{
    if (_web_socket) {
        detachWS();
    }
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
}
void Serial_2_Socket::begin(long speed)
{
    // echo("Serial2Socket::begin()");
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
}

void Serial_2_Socket::end()
{
    // echo("Serial2Socket::end()");
    _TXbufferSize = 0;
    _RXbufferSize = 0;
    _RXbufferpos = 0;
}

long Serial_2_Socket::baudRate()
{
    return 0;
}

bool Serial_2_Socket::attachWS(void * web_socket)
{
    // echo("Serial2Socket::attachWS()");
    if (web_socket) {
        _web_socket = web_socket;
        _TXbufferSize=0;
        return true;
    }
    return false;
}

bool Serial_2_Socket::detachWS()
{
    // echo("Serial2Socket::detachWS()");
    _web_socket = NULL;
    return true;
}

Serial_2_Socket::operator bool() const
{
    return true;
}
int Serial_2_Socket::available()
{
    // This is now calld for sure.
    // if(_RXbufferSize)
    //     echo("Serial2Socket::available() > 0");
    return _RXbufferSize;
}


size_t Serial_2_Socket::write(uint8_t c)
{
    if(!_web_socket) {
        return 0;
    }
    write(&c,1);
    return 1;
}

size_t Serial_2_Socket::write(const uint8_t *buffer, size_t size)
{
    // String debug =  String("Serial2Socket::write(buffer, ") + String(size);
    // echo(debug.c_str());
    if((buffer == NULL) ||(!_web_socket)) {
        if(buffer == NULL) {
            // echo("[SOCKET]No buffer");
            log_i("[SOCKET]No buffer");
        }
        if(!_web_socket) {
            // echo("[SOCKET]No Socket");
            log_i("[SOCKET]No socket");
        }
        return 0;
    }
#if defined(ENABLE_SERIAL2SOCKET_OUT)
    if (_TXbufferSize==0) {
        _lastflush = millis();
    }
    //send full line
    if (_TXbufferSize + size > TXBUFFERSIZE) {
        flush();
    }
    //need periodic check to force to flush in case of no end
    for (int i = 0; i < size; i++) {
        _TXbuffer[_TXbufferSize] = buffer[i];
        _TXbufferSize++;
    }
    log_i("[SOCKET]buffer size %d",_TXbufferSize);
    handle_flush();
#endif
    return size;
}

int Serial_2_Socket::peek(void)
{
    if (_RXbufferSize > 0) {
        // echo("Serial2Socket::peek()");
        return _RXbuffer[_RXbufferpos];
    } else {
        return -1;
    }
}

bool Serial_2_Socket::push (const char * data)
{
// echo("Serial2Soclet::push()");
#if defined(ENABLE_SERIAL2SOCKET_IN)
    int data_size = strlen(data);
    // String debug = String("Sz: ") + String(data_size);
    // debug += ", _RXbufferSize: " +String(_RXbufferSize);
    // debug += ", _RXbufferpos: " +String(_RXbufferpos); 
    // echo(debug.c_str());
    if ((data_size + _RXbufferSize) <= RXBUFFERSIZE) {
        int current = _RXbufferpos + _RXbufferSize;
        if (current > RXBUFFERSIZE) {
            current = current - RXBUFFERSIZE;
        }
        // String debug2 = String("Serial2Soclet::push(") + String(data) + ")";
        // echo(debug2.c_str());
        for (int i = 0; i < data_size; i++) {
            if (current > (RXBUFFERSIZE-1)) {
                current = 0;
            }
            _RXbuffer[current] = data[i];
            current ++;
        }
        _RXbufferSize+=strlen(data);
        return true;
    }
    return false;
#else
    return true;
#endif
}

int Serial_2_Socket::read(void)
{
    if (_RXbufferSize > 0) {
        int v = _RXbuffer[_RXbufferpos];
        _RXbufferpos++;
        if (_RXbufferpos > (RXBUFFERSIZE-1)) {
            _RXbufferpos = 0;
        }
        _RXbufferSize--;
        // String readEchoStr = String("Read(") + String((char)v) + ")";
        // echo(readEchoStr.c_str());
        return v;
    } else {
        return -1;
    }
}

void Serial_2_Socket::handle_flush()
{
    if (_TXbufferSize > 0) {
        if ((_TXbufferSize>=TXBUFFERSIZE) || ((millis()- _lastflush) > FLUSHTIMEOUT)) {
            log_i("[SOCKET]need flush, buffer size %d",_TXbufferSize);
            flush();
        }
    }
}
void Serial_2_Socket::flush(void)
{
    if (_TXbufferSize > 0) {
        log_i("[SOCKET]flush data, buffer size %d",_TXbufferSize);
        ((WebSocketsServer *)_web_socket)->broadcastBIN(_TXbuffer,_TXbufferSize);
        //refresh timout
        _lastflush = millis();
        //reset buffer
        _TXbufferSize = 0;
    }
}

#endif // ESP3D_WIFISUPPORT
