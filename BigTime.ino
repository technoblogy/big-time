/* Big Time Clock

   David Johnson-Davies - www.technoblogy.com - 28th January 2020
   ATtiny3216 @ 5 MHz (internal oscillator; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#define TWELVEHOUR                    // Comment out for 24-hour clock

// Seven segment displays

const int charArrayLen = 12;
uint8_t charArray[charArrayLen] = {
//  abcdefg  Segments
  0b1111110, // 0
  0b0110000, // 1
  0b1101101, // 2
  0b1111001, // 3
  0b0110011, // 4
  0b1011011, // 5
  0b1011111, // 6
  0b1110000, // 7
  0b1111111, // 8
  0b1111011, // 9
  0b0000000, // 10  Space
  0b0000001  // 11  '-'
};

const int Space = 10;
const int Dash = 11;

// Display multiplexer **********************************************

const int Ndigits = 4;
uint8_t Digits[Ndigits] = { Dash, Dash, Dash, Dash };
uint8_t Pins[Ndigits] = { 0, 1, 4, 5};                 // Port B output for each digit
int Digit = 0;

void DisplaySetup () {
  // Segment drivers
  PORTA.DIR = PIN4_bm | PIN5_bm | PIN6_bm | PIN7_bm;  // Set PA4-PA7 as outputs
  PORTC.DIR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;  // Set PC0-PC3 as outputs
  // Digit drivers
  PORTB.DIR = PIN0_bm | PIN1_bm | PIN4_bm | PIN5_bm;  // Set PB0 PB1 PB4 PB5 outputs
  PORTB.OUT = PIN0_bm | PIN1_bm | PIN4_bm | PIN5_bm;  // Set PB0 PB1 PB4 PB5 high
  // Set up Timer/Counter TCB to multiplex the display
  TCB0.CCMP = 19999;                                  // Divide 5MHz by 20000 = 250Hz
  TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm; // Enable timer, divide by 1
  TCB0.CTRLB = 0;                                     // Periodic Interrupt Mode
  TCB0.INTCTRL = TCB_CAPT_bm;                         // Enable interrupt
}

// Timer/Counter TCB interrupt - multiplexes the display
ISR(TCB0_INT_vect) {
  TCB0.INTFLAGS = TCB_CAPT_bm;                        // Clear the interrupt flag
  DisplayNextDigit();
}
  
void DisplayNextDigit() {
  PORTB.OUTSET = 1<<Pins[Digit];                      // Take digit high
  Digit = (Digit+1) % Ndigits;
  uint8_t segs = charArray[Digits[Digit]];
  PORTA.OUT = segs & 0xF0;                            // Set PA4-PA7
  PORTC.OUT = segs & 0x0F;                            // Set PC0-PC3
  PORTB.OUTCLR = 1<<Pins[Digit];                      // Take digit low
}

// Set time button **********************************************

int ButtonState = 0;

void ButtonSetup () {
  PORTA.PIN1CTRL = PORT_PULLUPEN_bm;                   // PA1 input pullup
}

boolean ButtonDown () {
  return (PORTA.IN & PIN1_bm) == 0;                    // True if button pressed
}

// Real-Time Clock **********************************************

volatile unsigned long Time = 0;                      // In half seconds

void RTCSetup () {
  uint8_t temp;
  // Initialize 32.768kHz Oscillator:

  // Disable oscillator:
  temp = CLKCTRL.XOSC32KCTRLA & ~CLKCTRL_ENABLE_bm;

  // Enable writing to protected register
  CPU_CCP = CCP_IOREG_gc;
  CLKCTRL.XOSC32KCTRLA = temp;

  while (CLKCTRL.MCLKSTATUS & CLKCTRL_XOSC32KS_bm);   // Wait until XOSC32KS is 0
  
  temp = CLKCTRL.XOSC32KCTRLA & ~CLKCTRL_SEL_bm;      // Use External Crystal
  
  // Enable writing to protected register
  CPU_CCP = CCP_IOREG_gc;
  CLKCTRL.XOSC32KCTRLA = temp;
  
  temp = CLKCTRL.XOSC32KCTRLA | CLKCTRL_ENABLE_bm;    // Enable oscillator
  
  // Enable writing to protected register
  CPU_CCP = CCP_IOREG_gc;
  CLKCTRL.XOSC32KCTRLA = temp;
  
  // Initialize RTC
  while (RTC.STATUS > 0);                             // Wait until registers synchronized

  // 32.768kHz External Crystal Oscillator (XOSC32K)
  RTC.CLKSEL = RTC_CLKSEL_TOSC32K_gc;
  
  RTC.DBGCTRL = RTC_DBGRUN_bm; // Run in debug: enabled

  RTC.PITINTCTRL = RTC_PI_bm;                         // Periodic Interrupt: enabled
  
  // RTC Clock Cycles 16384, enabled ie 2Hz interrupt
  RTC.PITCTRLA = RTC_PERIOD_CYC16384_gc | RTC_PITEN_bm;
}

// Interrupt Service Routine called twice a second
ISR(RTC_PIT_vect) {
  int minutes, hours;
  RTC.PITINTFLAGS = RTC_PI_bm;                        // Clear interrupt flag
  minutes = (Time / 120) % 60;
#ifdef TWELVEHOUR
  hours = (Time / 7200) % 12;
#elif
  hours = (Time / 7200) % 24;
#endif
  if (ButtonDown()) {
    if (ButtonState == 1 || ButtonState == 3) {
        ButtonState = (ButtonState + 1) % 4;
    }
    if (ButtonState == 0) {                           // Advance hours
#ifdef TWELVEHOUR
      hours = (hours + 1) % 12;
#elif
      hours = (hours + 1) % 24;
#endif
    } else {                                          // Advance minutes
      minutes = (minutes + 1) % 60;
    }
    Time = (unsigned long)hours * 7200 + minutes * 120;
  } else {                                            // Button up
     if (ButtonState == 0 || ButtonState == 2) {
        ButtonState = (ButtonState + 1) % 4;
     }
     Time = (Time + 1) % 172800;                       // Wrap around after 24 hours
  }
#ifdef TWELVEHOUR
  hours = hours + 1;
#endif
  Digits[0] = hours/10;
  Digits[1] = hours%10;
  Digits[2] = minutes/10;
  Digits[3] = minutes%10;              
}

// Setup **********************************************

void setup () {
  DisplaySetup();
  RTCSetup();
  ButtonSetup();
}

// Everything done under interrupt
void loop () {
}