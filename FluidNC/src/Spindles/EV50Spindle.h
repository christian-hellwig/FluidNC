// Copyright (c) 2024 -	Christian Hellwig
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#pragma once

#include "VFDSpindle.h"

namespace Spindles {
    class EV50Spindle : public VFD {
    private:
        int reg;

    protected:
        uint16_t _minFrequency = 0;
        uint16_t _maxFrequency = 4000;  // EV50 works with frequencies scaled by 10.

        void updateRPM();

        void direction_command(SpindleState mode, ModbusCommand& data) override;
        void set_speed_command(uint32_t rpm, ModbusCommand& data) override;

        response_parser initialization_sequence(int index, ModbusCommand& data) override;
        response_parser get_status_ok(ModbusCommand& data) override { return nullptr; }
        response_parser get_current_speed(ModbusCommand& data) override;

        bool use_delay_settings() const override { return false; }

    public:
        EV50Spindle(const char* name) : VFD(name) {}
    };
}
