// Copyright (c) 2024 -	Christian Hellwig
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

/*
   EV50Spindle.cpp

    This is for a EV50 VFD based spindle via RS485 Modbus.

                         WARNING!!!!
    VFDs are very dangerous. They have high voltages and are very powerful
    Remove power before changing bits.
*/

#include "EV50Spindle.h"

#include <algorithm>   // std::max
#include <esp_attr.h>  // IRAM_ATTR

namespace Spindles {
    void EV50Spindle::direction_command(SpindleState mode, ModbusCommand& data) {
        // NOTE: data length is excluding the CRC16 checksum.
        data.tx_length = 6;
        data.rx_length = 6;

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = 0x06;
        data.msg[2] = 0x01;
        data.msg[3] = 0x22;
        data.msg[4] = 0x00;

        switch (mode) {
            case SpindleState::Cw:  //[01] [06] [01 22] [00 01] -- forward run
                data.msg[5] = 0x01;
                break;
            case SpindleState::Ccw:  //[01] [06] [01 22] [00 04] -- reverse run
                data.msg[5] = 0x04;
                break;
            default:  // SpindleState::Disable [01] [06] [01 22] [00 10] -- stop
                data.msg[5] = 0x10;
                break;
        }
    }

    void IRAM_ATTR EV50Spindle::set_speed_command(uint32_t dev_speed, ModbusCommand& data) {
        data.tx_length = 6;
        data.rx_length = 6;

        if (dev_speed != 0 && (dev_speed < _minFrequency || dev_speed > _maxFrequency)) {
            log_warn(name() << " requested freq " << (dev_speed) << " is outside of range (" << _minFrequency << "," << _maxFrequency << ")");
        }

#ifdef DEBUG_VFD
        log_debug("Setting VFD dev_speed to " << dev_speed);
#endif

        //[01] [06] [0121] [1388] Set frequency 50% of max frequency [1388] = 200.0 Hz. (5000 is written!)

        // calculate value in msg

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = 0x06;  // Set register command
        data.msg[2] = 0x01;
        data.msg[3] = 0x21;
        data.msg[4] = (_maxFrequency / dev_speed *10000) >> 8;
        data.msg[5] = (_maxFrequency / dev_speed *10000) & 0xFF;
    }

    // This gets data from the VFD. It does not set any values
    VFD::response_parser EV50Spindle::initialization_sequence(int index, ModbusCommand& data) {
        // NOTE: data length is excluding the CRC16 checksum.
        data.tx_length = 6;
        data.rx_length = 5;
        // Read P02.19 (min frequency) and P02.18 (max frequency):
        //
        // [01] [03] [00 da] [00 02] [e5 fo] min frequency 
        // [01] [03] [00 d9] [00 02] [15 fo] max frequency 

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = 0x03;  // Read setting
        data.msg[2] = 0x00;
        //      [3] = set below...
        data.msg[4] = 0x00;  // length
        data.msg[5] = 0x02;

        if (index == -1) {
            // Max frequency
            data.msg[3] = 0xd9;  // P02.18 (max frequency) the VFD will allow. Normally 400.

            return [](const uint8_t* response, Spindles::VFD* vfd) -> bool {
                uint16_t value = (response[3] << 8) | response[4];

#ifdef DEBUG_VFD
                log_debug("VFD: Max frequency = " << value / 10 << "Hz " << value / 10 * 60 << "RPM");
#endif
                log_info("VFD: Max speed:" << (value / 10 * 60) << "rpm");

                // Set current RPM value? Somewhere?
                auto ev50           = static_cast<EV50Spindle*>(vfd);
                ev50->_maxFrequency = value;

                return true;
            };

        } else if (index == -2) {
            // Min Frequency
            data.msg[3] = 0xda;  // P02.19 (min frequency) lower limit. Normally 0.

            return [](const uint8_t* response, Spindles::VFD* vfd) -> bool {
                uint16_t value = (response[3] << 8) | response[4];

#ifdef DEBUG_VFD
                log_debug("VFD: Min frequency = " << value / 10 << "Hz " << value / 10 * 60 << "RPM");
#endif
                log_info("VFD: Min speed:" << (value / 10 * 60) << "rpm");

                // Set current RPM value? Somewhere?
                auto ev50           = static_cast<EV50Spindle*>(vfd);
                ev50->_minFrequency = value;

                ev50->updateRPM();

                return true;
            };
        }

        // Done.
        return nullptr;
    }

    void EV50Spindle::updateRPM() {
        if (_minFrequency > _maxFrequency) {
            _minFrequency = _maxFrequency;
        }

        if (_speeds.size() == 0) {
            SpindleSpeed minRPM = _minFrequency * 60 / 10;
            SpindleSpeed maxRPM = _maxFrequency * 60 / 10;

            shelfSpeeds(minRPM, maxRPM);
        }
        setupSpeeds(_maxFrequency);
        _slop = std::max(_maxFrequency / 40, 1);

        log_info("VFD: VFD settings read: Freq range(" << _minFrequency << " , " << _maxFrequency << ")]");
    }

    VFD::response_parser EV50Spindle::get_current_speed(ModbusCommand& data) {
        // NOTE: data length is excluding the CRC16 checksum.
        // [01] [03] [03 fc] [00 02] [04 7f] -- output frequency
        data.tx_length = 6;
        data.rx_length = 7;

        // data.msg[0] is omitted (modbus address is filled in later)
        data.msg[1] = 0x03;
        data.msg[2] = 0x03;
        data.msg[3] = 0xfc;  // Output frequency
        data.msg[4] = 0x00;
        data.msg[5] = 0x02;

        return [](const uint8_t* response, Spindles::VFD* vfd) -> bool {
            //? 01 04 04 [freq 16] [set freq 16] [crc16]
            uint16_t frequency = (uint16_t(response[3]) << 8) | uint16_t(response[4]);

            // Store speed for synchronization
            vfd->_sync_dev_speed = frequency;
            return true;
        };
    }

    // Configuration registration
    namespace {
        SpindleFactory::InstanceBuilder<EV50Spindle> registration("EV50");
    }
}
