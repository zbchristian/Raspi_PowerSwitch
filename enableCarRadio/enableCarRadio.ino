/*
  Enable Car Radio/Moniceiver even when ignition is off with Digispark ATTINY85
  =========================================================================================
  Car radios are provided with two differnt 12V power lines:
    - 12V permanent power
    - auxiliary power. which is off, when the ignition is switched off
  Most radios switch off and can not be turned on, when the ignition is off (no aux power). 
  Internal setting are saved by means of the permanent power

  This sketch provides the possibility to turn the radio on for a predefined maximal time (e.g. 30min or 1h). 
  This is achieved by triggering a MOSFET switch (or relais) from the ATTINY to provide the aux power from permanent 12V.
  The on time should be short enough in order to not drain the battery!!
  
  The external trigger, to enable the radio can be
  - A hardware switch (push button)
  - the ignition is briefly turned on (engine NOT started) and off again within a short time (e.g. 5 seconds).

  This sketch contains another function: the ambient light is measured with a sensor and the display of the Moniceiver 
  can be set to day/night mode. For this the device has to have a corresponding input (e.g Pioneer moniceivers provide a cable for the light switch)

  Required Hardware: 
    - Digispark ATTINY85 board
      The onboard voltage regulator (78L05 or 78M05) is provided with the cars 12V (Vin connection)
    - MOSFET switch or relais to connect aux power of the radio to permanent 12V. Needs to be switched by the GPIO pins (approx. 4V) of the ATTINY85
    - ambient light sensor, which has the sensitivity of the human eye
    - transistor to trigger the "lights on" signal (12V)
    - connectors, cables, switch and housing for the setup

  The processor runs with a reduced clock speed to save power. Be aware, that this screws up the 
  the standard delay() function. Use delayms() instead, which uses a corrected timer.
  The Bootloader runs at 16.5MHz. The clock speed is reduced by means of the clock prescaler (CLK_PRESC) down to approx. 1MHz
  Timer setup is automatically adjusted to the chosen prescale factor

  REMARK concerning the power consumption of the Arduino board:
  At the reduced speed the current required by the CPU is of the order few mA (below 30mW). Each of the onboard LEDs require about the same power.
  5mA per LED (1k resistor). In order to reduce the power consumption, either remove the corresponding resistors, or replace them with a 10k version 
  (only 0.5mA current and still visible LED).
  
  C. Zeitnitz 2023
*/
#include <avr/power.h>
#include <CapacitiveSensor.h>

#define CLK_PRESC 16                // range 1-32 : automatic adjustment of CPU clock and timer setup
#define CPUClk    16.5/CLK_PRESC    // effective clock frequency in MHz - 16.5 MHz with a prescaler of 16 yields 1.03 MHz ( see setup() )

#if CLK_PRESC == 2
  #define CLOCK_DIV clock_div_2 
#elif CLK_PRESC == 4
  #define CLOCK_DIV clock_div_4 
#elif CLK_PRESC == 8
  #define CLOCK_DIV clock_div_8
#elif CLK_PRESC == 16
  #define CLOCK_DIV clock_div_16
#elif CLK_PRESC == 32
  #define CLOCK_DIV clock_div_32
#else
  #define CLOCK_DIV clock_div_1
#endif

#define buttonPin    0   // switch power of Raspi on/off (LOW/HIGH in case of n-channel) 

#define MosFET    0   // switch power of Raspi on/off (LOW/HIGH in case of n-channel) 
#define Shutdown  1   // inform Raspi to shutdown - put green LED in series to adjust to 3.3V  
#define raspHalt  3   // external pullup (1k5) and 3V Zener - Raspi sets this pin to (LOW) when running. Pulled to HIGH after shutdown 
#define CapOut    4   // connect 10MOhm to touch surface (small metal piece)
#define CapIn     2   // connect directly to touch surface
#define CapThresh 20  // threshold to detect, that metal surface has been touched. NEEDS ADJUSTMENT! 

#define time2Boot  45  // time for the raspi to boot (sec)
#define time2Halt  15  // time for the raspi to halt after shutdown notice (sec)


CapacitiveSensor but=CapacitiveSensor(CapOut,CapIn);

volatile enum {OFF,BOOT,RUNNING,HALT} state;

volatile int  delayMillis = 0;
volatile bool isDelay = false;

// the setup function runs once when you press reset or power the board
void setup() {
  clock_prescale_set(CLOCK_DIV);  // the program should go slow - save energy
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(buttonPin, HIGH);
  pinMode(MosFET, OUTPUT);
  digitalWrite(MosFET, LOW);      // turn power for Raspi OFF
  setupTimer();
  state = BOOT;
  delayms(1000);
  digitalWrite(MosFET, HIGH);   // turn power for Raspi ON -> boot
}

// check the button once per loop and act accordingly
void loop() {
  bool isON = (digitalRead(MosFET) == HIGH);
  bool isRequest = (state == RUNNING || state == OFF) && buttonActive();          // button pressed -> switch raspi ON/OFF 
  isRequest = isRequest || (state == RUNNING && digitalRead(raspHalt) == HIGH);   // raspi is shutting down on its own
  if ( isRequest ) {                            // check button/raspi state, if not in boot or halt transition
    if (isON && state == RUNNING) {             // Raspi is ON, but not yet in transition (boot or halt)
      togglePIN(Shutdown,300);                  // request raspi shutdown
      state = HALT;                             // halt process is starting
    } else if (!isON) {                         // raspi is OFF
      digitalWrite(MosFET, HIGH);               // turn power for Raspi ON
      state = BOOT;                             // boot process is starting
    }
  }
  delayms(100); // sample button every 100ms
}

bool buttonActive() {
  static int  npressed = 0;
  bool        isAct = false;
  if (digitalRead(buttonPin) == LOW) {
      ++npressed;
      if (npressed>3) {
        isAct = true;
        npressed=0;
      }
  }
  else npressed=0;  
  return isAct;
}

int getAmbientBrightness() {

}

volatile unsigned int delayTime = 0;
volatile unsigned int bootTime = 0;
volatile unsigned int haltTime = 0;
volatile unsigned int timeSecond = 0;
volatile bool isSecond = false;

// Timer1 interrupt
// Timing of delayms, boot and halt
// Called once every 1msec
ISR(TIMER1_COMPA_vect) 
{
  ++timeSecond;
  isSecond = (timeSecond%1000) == 0;  // 
  
  delayTime = isDelay ? delayTime+1 : 0;
  if (isDelay && (delayTime%delayMillis) == 0) isDelay = false;

  if (isSecond) {   // boot and halt timing in seconds
    haltTime = state==HALT ? haltTime+1 : 0;
    if (state==HALT && (haltTime%time2Halt) == 0 ) {
      digitalWrite(MosFET, LOW);   // turn power for Raspi OFF
      state = OFF;    
    }
    bootTime = state==BOOT ? bootTime+1 : 0;
    if ( state==BOOT && (bootTime%time2Boot) == 0 ) {
      state = RUNNING;
    }
  }
}

// Timer setup adjust automatically to the system clock (CLK_PRESC)
// setup timer1 in count and compare/match (CTC) and create interrupt once every msec
void setupTimer() {
  noInterrupts();               // disable all interrupts
  TCNT1   = 0;
  TCCR1   = 0b10000000;         // CTC1=1, COM1A1=0, COM1A0=0, last 4 bits is prescale factor
  // prescaler: 0001=1; 0010=2; 0011=4; 0100=8; 0101=16; ... (2^(cs_bits-1)) - adjust to CPU clock
  int presc = 128/CLK_PRESC;    // prescaler 128 for 16.5 MHz and just divide by prescale factor of the CPU clock
  byte cs_bits = (byte)(log(presc)/log(2)+1.5); // get bit patterns of prescale value (0.5 is added for rounding)
  TCCR1  |= cs_bits;
  TIMSK   = _BV(OCIE1A);        // enable timer and set interrupt to compare
  OCR1C   = (byte) (( (CPUClk*1000.0) / presc)+0.5); // calculate counter value for the interrupt (max 255) 
  interrupts();                 // enable all interrupts
}

void togglePIN (int pin, int msecs) {
  int pstate = digitalRead(pin);
  if(pstate == HIGH) digitalWrite(pin, LOW); 
  else digitalWrite(pin, HIGH);
  if (msecs > 0) {
    delayms(msecs);
    digitalWrite(pin, pstate);
  }
}

void delayms(int msecs) {
  delayMillis = msecs;
  isDelay = true;
  while ( isDelay );
}
