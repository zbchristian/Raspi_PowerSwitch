/*
  Raspberry Pi Power Switch with Digispark ATTINY85
  =================================================
  Port 0 : control  p-channel power MOSFET
  Port 1 : toggle shutdown pin of Raspi (active high) 
  Port 2 : receive pin for touch button
  Port 3 : receive shutdown info from raspi (LOW)
  Port 4 : send pin for touchbutton

  How does the code work:
  - Button (touch surface) triggers the start/shutdown of the raspberry pi
  - Power is connected/disconnected from the raspi by means of a MOSFET ( p-channel with RDS(on)<=0.1 Ohm to diconnect +5V from Raspi ) 
  - after disconnecting, the remaining current is a few mA
  - All timing is done in the Timer1 interrupt routine
  - Main loop samples the button, sets flags and acts accordingly (request shutdown/power on the raspi)

  The processor runs with a reduced clock speed to save power. Be aware, that this screws up the 
  the standard delay() function. Use delayms() instead, which uses a corrected timer.

  Required Hardware: Digispark ATTINY85 board
                     - BE CAREFUL: the ATTINY runs at 5V and the Raspberry PI only 3.3V. The voltage levels should be adjusted when connecting GPIO pins.
                       o From ATTINY to GPIO: add a green LED, which reduces the output voltage by approx 2V
                       o From GPIO to ATTINY: depends on the pin. An input pin can directly be attached (no pull-up!). A 10k series resistor is recommended
                         to avoid current flow in shutdown state. Pin 3 of the Digispark has a Zener diode and pull-up attached. This limits the voltage 
                         already to 3V. The GPIO should pull Pin 3 to ground only and best over a standard diode.
                       o Add 
                     - The Bootloader runs at 16.5MHz. The clock speed is reduced by means of the clock prescaler (CLK_PRESC) down to approx. 1MHz
                       Timer setup is automatically adjusted to the chosen prescale factor

  Raspberry Pi settings: Configure two GPIOs in /boot/config.txt
                     - Shutdown: set to HIGH and raspi will start shutdown: 
                       dtoverlay=gpio-shutdown,gpio_pin=17,active_low=0,gpio_pull=down  
                     - Set pin to LOW when starting up. After a shutdown the GPIO is undefined. Pullup sets it to HIGH -> power can be switched off:
                       gpio=27=op,dl
                        
  Required package: CapacitiveSensor

  C. Zeitnitz 2021
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

#define MosFET    0   // switch power of Raspi on/off (HIGH/LOW in case of p-channel FET) 
#define Shutdown  1   // inform Raspi to shutdown - put green LED in series to adjust to 3.3V  
#define raspHalt  3   // external pullup (1k5) and 3V Zener - Raspi sets this pin to (LOW) when running. Pulled to HIGH after shutdown 
#define CapOut    4   // connect 1-10MOhm to touch surface (small metal piece)
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
  pinMode(MosFET, OUTPUT);
  digitalWrite(MosFET, LOW);      // turn power for Raspi OFF
  pinMode(Shutdown, OUTPUT);
  digitalWrite(Shutdown, LOW);
  pinMode(raspHalt, INPUT);
  digitalWrite(raspHalt, HIGH);
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
  static int  ntouch = 0;
  long        cs = but.capacitiveSensor(50);
  bool        isAct = false;
  if (cs > CapThresh) {
      ++ntouch;
      if (ntouch>2) {
        isAct = true;
        ntouch=0;
      }
  }
  else ntouch=0;  
  return isAct;
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
  TIMSK   = _BV(OCIE1A);        // enable timer compare interrupt for compare
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
