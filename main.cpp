#include "mbed.h"

// Define shield connections (NUCLEO-F401RE pins)
DigitalOut latch(PB_5);    // Latch Pin (D4)
DigitalOut clk(PA_8);      // Clock Pin (D7)
DigitalOut data(PA_9);     // Data Pin (D8)
DigitalIn  button1(PA_1);  // Reset button Pin (S1)
DigitalIn  button3(PB_0);  // Mode button Pin (S3)
AnalogIn   pot(PA_0);      // Potentiometer Pin (A0)

// 7-segment encoding (Common Anode – active low)
const uint8_t SEGMENT_MAP[10] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99,
    0x92, 0x82, 0xF8, 0x80, 0x90
};

// Digit selector (active low, 4-digit)
const uint8_t SEGMENT_SELECT[4] = { 0xF1, 0xF2, 0xF4, 0xF8 };

// Shared state
volatile int suh = 0;                  // Counter for total seconds
volatile bool su_disp = true;         // Display update flag
Ticker second_tick;
Ticker refresh_tick;
volatile int su_digit = 0;            // Current digit to refresh

// ISR: Increment seconds counter every 1 sec
void suhaila_tick() {
    suh++;
    if (suh >= 6000) suh = 0;  // Wrap around at 99:59
}

// ISR: Trigger display refresh flag every 2 ms
void suhaila_refresh() {
    su_disp = true;
}

// Send data to 74HC595 shift registers
void suhaila_out(uint8_t segments, uint8_t digitSelect) {
    latch = 0;
    for (int i = 7; i >= 0; --i) {
        data = (segments >> i) & 0x1;
        clk = 0; clk = 1;
    }
    for (int i = 7; i >= 0; --i) {
        data = (digitSelect >> i) & 0x1;
        clk = 0; clk = 1;
    }
    latch = 1;
}

int main() {
    button1.mode(PullUp);
    button3.mode(PullUp);

    second_tick.attach(&suhaila_tick, 1.0);          // Tick every 1 sec
    refresh_tick.attach(&suhaila_refresh, 0.002);    // Refresh display every 2 ms

    bool modeVoltage = false;
    int prev_b1 = 1, prev_b3 = 1;

    while (true) {
        // Button S1: Reset time
        int b1 = button1.read();
        if (b1 == 0 && prev_b1 == 1) {
            suh = 0;
        }
        prev_b1 = b1;

        // Button S3: Switch display mode (hold to show voltage)
        int b3 = button3.read();
        modeVoltage = (b3 == 0);
        prev_b3 = b3;

        // Refresh display
        if (su_disp) {
            su_disp = false;

            uint8_t segByte = 0xFF, selByte = 0xFF;

            if (!modeVoltage) {
                // Display MM:SS
                int seconds = suh % 60;
                int minutes = suh / 60;

                switch (su_digit) {
                    case 0: segByte = SEGMENT_MAP[minutes / 10]; selByte = SEGMENT_SELECT[0]; break;
                    case 1: segByte = SEGMENT_MAP[minutes % 10] & 0x7F; selByte = SEGMENT_SELECT[1]; break; // colon
                    case 2: segByte = SEGMENT_MAP[seconds / 10]; selByte = SEGMENT_SELECT[2]; break;
                    case 3: segByte = SEGMENT_MAP[seconds % 10]; selByte = SEGMENT_SELECT[3]; break;
                }
            } else {
                // Display voltage X.XXX format
                float volts = pot.read() * 3.3f;
                int millivolts = (int)(volts * 1000.0f);
                if (millivolts > 9999) millivolts = 9999;

                int intPart = millivolts / 1000;
                int fracPart = millivolts % 1000;

                switch (su_digit) {
                    case 0: segByte = SEGMENT_MAP[intPart] & 0x7F; selByte = SEGMENT_SELECT[0]; break; // decimal
                    case 1: segByte = SEGMENT_MAP[fracPart / 100]; selByte = SEGMENT_SELECT[1]; break;
                    case 2: segByte = SEGMENT_MAP[(fracPart % 100) / 10]; selByte = SEGMENT_SELECT[2]; break;
                    case 3: segByte = SEGMENT_MAP[fracPart % 10]; selByte = SEGMENT_SELECT[3]; break;
                }
            }

            suhaila_out(segByte, selByte);
            su_digit = (su_digit + 1) % 4;
        }
    }
}