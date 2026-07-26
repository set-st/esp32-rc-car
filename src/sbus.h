#ifndef SBUS_H
#define SBUS_H

#include <Arduino.h>

class SBUS {
public:
    static const int NUM_CHANNELS = 16;
    
    SBUS() : _failsafe(true), _frameLost(false), _lastUpdate(0), _bufferIdx(0) {
        memset(_channels, 0, sizeof(_channels));
        memset(_buffer, 0, sizeof(_buffer));
    }

    void begin(int rxPin) {
        // SBUS is 100000 baud, 8E2, inverted.
        // On ESP32, we can configure any RX pin and enable signal inversion.
        Serial2.begin(100000, SERIAL_8E2, rxPin, -1, true);
    }

    bool read() {
        bool newFrame = false;
        while (Serial2.available() > 0) {
            uint8_t c = Serial2.read();
            
            // Sync: SBUS packet must start with 0x0F
            if (_bufferIdx == 0 && c != 0x0F) {
                continue;
            }

            _buffer[_bufferIdx++] = c;

            if (_bufferIdx == 25) {
                // An SBUS packet has exactly 25 bytes.
                // The end byte is typically 0x00, but can have flags.
                // We validate the frame structure here.
                if (_buffer[24] == 0x00 || (_buffer[24] & 0x0F) == 0x00) {
                    decode();
                    newFrame = true;
                    _lastUpdate = millis();
                } else {
                    // Invalid end byte, reset sync
                    _bufferIdx = 0;
                }
            }
        }

        // Failsafe if no frames received in the last 500ms
        if (millis() - _lastUpdate > 500) {
            _failsafe = true;
        }

        return newFrame;
    }

    uint16_t getChannel(int channel) const {
        if (channel >= 0 && channel < NUM_CHANNELS) {
            return _channels[channel];
        }
        return 1023; // Default midpoint for 11-bit channel (0 - 2047)
    }

    bool isFailsafe() const {
        return _failsafe;
    }

    bool isFrameLost() const {
        return _frameLost;
    }

private:
    uint8_t _buffer[25];
    int _bufferIdx;
    uint16_t _channels[NUM_CHANNELS];
    bool _failsafe;
    bool _frameLost;
    uint32_t _lastUpdate;

    void decode() {
        // Decode 11-bit channels from packet buffer
        _channels[0]  = ((_buffer[1]      | _buffer[2]<<8)                       & 0x07FF);
        _channels[1]  = ((_buffer[2]>>3   | _buffer[3]<<5)                       & 0x07FF);
        _channels[2]  = ((_buffer[3]>>6   | _buffer[4]<<2 | _buffer[5]<<10)      & 0x07FF);
        _channels[3]  = ((_buffer[5]>>1   | _buffer[6]<<7)                       & 0x07FF);
        _channels[4]  = ((_buffer[6]>>4   | _buffer[7]<<4)                       & 0x07FF);
        _channels[5]  = ((_buffer[7]>>7   | _buffer[8]<<1 | _buffer[9]<<9)       & 0x07FF);
        _channels[6]  = ((_buffer[9]>>2   | _buffer[10]<<6)                      & 0x07FF);
        _channels[7]  = ((_buffer[10]>>5  | _buffer[11]<<3)                      & 0x07FF);
        _channels[8]  = ((_buffer[12]     | _buffer[13]<<8)                      & 0x07FF);
        _channels[9]  = ((_buffer[13]>>3  | _buffer[14]<<5)                      & 0x07FF);
        _channels[10] = ((_buffer[14]>>6  | _buffer[15]<<2 | _buffer[16]<<10)    & 0x07FF);
        _channels[11] = ((_buffer[16]>>1  | _buffer[17]<<7)                      & 0x07FF);
        _channels[12] = ((_buffer[17]>>4  | _buffer[18]<<4)                      & 0x07FF);
        _channels[13] = ((_buffer[18]>>7  | _buffer[19]<<1 | _buffer[20]<<9)     & 0x07FF);
        _channels[14] = ((_buffer[20]>>2  | _buffer[21]<<6)                      & 0x07FF);
        _channels[15] = ((_buffer[21]>>5  | _buffer[22]<<3)                      & 0x07FF);

        // Flags byte (byte 23)
        _frameLost = (_buffer[23] & 0x04) != 0;
        _failsafe  = (_buffer[23] & 0x08) != 0;

        _bufferIdx = 0; // Ready for next frame
    }
};

#endif
