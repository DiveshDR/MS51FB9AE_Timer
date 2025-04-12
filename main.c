#include "MS51_16K.H"

// Segment pins
sbit SEG_A = P1^3;
sbit SEG_B = P0^0;
sbit SEG_C = P1^5;
sbit SEG_D = P0^3;
sbit SEG_E = P0^1;
sbit SEG_F = P1^2;
sbit SEG_G = P1^7;
sbit SEG_DP = P0^4;

// Digit pins
sbit DIGIT1 = P1^4;
sbit DIGIT2 = P1^1;
sbit DIGIT3 = P1^0;

// Relay pin
sbit RELAY = P0^6; // High=OFF (NC), Low=ON (NO)

// Segment patterns (0–9, S, O, F, N)
const unsigned char patterns[14] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x6D, // S
    0x3F, // O
    0x71, // F
    0x37  // N
};

// Delay (~16MHz)
void delay_ms(unsigned char ms) {
    unsigned char i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 160; j++);
}

// Display character
void set_segments(char c) {
    unsigned char seg = 0;
    if (c >= '0' && c <= '9') seg = patterns[c - '0'];
    else if (c == 'S') seg = patterns[10];
    else if (c == 'O') seg = patterns[11];
    else if (c == 'F') seg = patterns[12];
    else if (c == 'N') seg = patterns[13];
    SEG_A = !(seg & 0x01); SEG_B = !(seg & 0x02);
    SEG_C = !(seg & 0x04); SEG_D = !(seg & 0x08);
    SEG_E = !(seg & 0x10); SEG_F = !(seg & 0x20);
    SEG_G = !(seg & 0x40); SEG_DP = 1;
}

void main(void) {
    // Variables
    unsigned char display[3]; // Buffer
    unsigned char seconds = 30; // Default 30s
    bit relay_on = 0;         // 0=OFF, 1=ON
    bit setup_mode = 0;       // 0=Normal, 1=Setup
    unsigned char menu = 0;   // 0=Sec, 1=ON/OFF
    unsigned int timer = 0;   // ~1s counter
    unsigned char count = 30; // Countdown
    unsigned char btn_timer = 0; // Debounce/hold
    unsigned char last_btn = 0; // Last button

    // Configure Port 0 (P0.0, P0.1, P0.3, P0.4, P0.6)
    P0M1 &= ~0x5B;
    P0M2 |= 0x5B;

    // Configure Port 1 (P1.0, P1.1, P1.2, P1.3, P1.4, P1.5, P1.7)
    P1M1 &= ~0xBF;
    P1M2 |= 0xBF;

    // Configure P0.7 (ADC)
    P0M1 |= 0x80;
    P0M2 &= ~0x80;
    AINDIDS = 0x80;

    // Configure ADC
    ADCCON0 = 0x02;
    ADCCON1 = 0x01;

    // Initialize
    RELAY = 1; // OFF (NC)
    DIGIT1 = 0;
    DIGIT2 = 0;
    DIGIT3 = 0;

    while (1) {
        unsigned int adc_value;
        unsigned char btn = 0, i;

        // Read ADC
        ADCCON0 |= 0x40;
        while (!(ADCCON0 & 0x80));
        ADCCON0 &= ~0x80;
        adc_value = (unsigned int)((ADCRH << 2) | (ADCRL >> 6));

        // Detect buttons
        if (adc_value >= 133 && adc_value <= 194) btn = 1; // LEFT (0.8V)
        else if (adc_value >= 297 && adc_value <= 358) btn = 2; // MIDDLE (1.6V)
        else if (adc_value >= 481 && adc_value <= 542) btn = 3; // RIGHT (2.5V)

        // Button logic
        if (btn == last_btn) {
            btn_timer++;
            if (btn_timer >= 4 && btn) { // ~20ms
                if (btn == 2 && btn_timer >= 140) { // ~700ms
                    setup_mode = !setup_mode;
                    menu = 0;
                    btn_timer = 0;
                    count = seconds;
                }
            }
        } else {
            if (btn_timer >= 4 && btn_timer < 40 && last_btn) { // Click
                if (setup_mode) {
                    if (last_btn == 2) menu = !menu;
                    else if (last_btn == 1) {
                        if (menu == 0 && seconds > 1) seconds--;
                        else if (menu == 1) relay_on = 0;
                    } else if (last_btn == 3) {
                        if (menu == 0 && seconds < 60) seconds++;
                        else if (menu == 1) relay_on = 1;
                    }
                }
            }
            btn_timer = 0;
            last_btn = btn;
        }

        // Timer
        if (!setup_mode) {
            timer++;
            if (timer >= 200) { // ~1s
                timer = 0;
                if (count > 0) {
                    count--;
                    if (count == 0) {
                        RELAY = !relay_on;
                        count = seconds;
                    }
                }
            }
        }

        // Update display
        if (setup_mode) {
            if (menu == 0) { // Seconds
                display[0] = 'S';
                display[1] = '0' + (seconds / 10);
                display[2] = '0' + (seconds % 10);
            } else { // ON/OFF
                display[0] = 'O';
                display[1] = relay_on ? 'N' : 'F';
                display[2] = ' ';
            }
        } else {
            display[0] = 'S';
            display[1] = '0' + (count / 10);
            display[2] = '0' + (count % 10);
        }

        // Multiplex (~66Hz)
        for (i = 0; i < 5; i++) {
            set_segments(display[0]);
            DIGIT1 = 1; DIGIT2 = 0; DIGIT3 = 0;
            delay_ms(5);
            DIGIT1 = 0;

            set_segments(display[1]);
            DIGIT2 = 1; DIGIT1 = 0; DIGIT3 = 0;
            delay_ms(5);
            DIGIT2 = 0;

            set_segments(display[2]);
            DIGIT3 = 1; DIGIT1 = 0; DIGIT2 = 0;
            delay_ms(5);
            DIGIT3 = 0;
        }
    }
}
