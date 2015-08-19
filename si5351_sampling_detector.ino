#include <Wire.h> 
#include "si5351.h"

// From http://arduino-info.wikispaces.com/LCD-Blue-I2C#v1
#include <LiquidCrystal_I2C.h>

// define LCD pinouts: addr,en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// encoder definitions
#define ENCODER_A A0
#define ENCODER_B A1
#define ENCODER_BUTTON A2                            
#define ENCODER_INPUT_PORT PINC
#define BAND_SWITCH A3

// synthesiser 3.5Mhz-3.9MHz (80m band)
long frequency = 3500000;                    // 4 bytes required for frequency
long step_size = 1000;                       // default tuning increment (Hz)
long encoder;                                // encoder direction: CCW=-1; NC=0; CW=1
Si5351 clockgen;                             // instantiate clockgen

/******************************
 setup
*******************************/
void setup() { 
  Serial.begin(115200);                      // set serial port speed
  lcd.begin(16,2);                           // 16 char x 2 line LCD display
  display_freq_step();                             // display frequency & step-size
  
  // configure rotary encoder & push-button
  pinMode(ENCODER_A, INPUT_PULLUP);          // WIRE-OR encoder leads
  pinMode(ENCODER_B, INPUT_PULLUP);  
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  pinMode(BAND_SWITCH, INPUT_PULLUP);
  
  // set clk0 output to 3.5MHz with a fixed PLL frequency
  clockgen.init(SI5351_CRYSTAL_LOAD_8PF);
  clockgen.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  clockgen.set_freq(frequency, SI5351_PLL_FIXED, SI5351_CLK0);
}

/******************************
 main loop
*******************************/
void loop() {
  check_band();
  check_step_size();
  
  // check the frequency 
  encoder = read_encoder();                       // returns +1/0/-1 (cw/nc/ccw)
  if (encoder != 0) {
    frequency = frequency + (encoder * step_size);
    clockgen.set_freq((frequency * 2), SI5351_PLL_FIXED, SI5351_CLK0);
                                                 // sampling detector LO twice signal frequency
    display_freq_step();                         // display frequency & step-size
  } 
}

/******************************
 read encoder direction
*******************************/
char read_encoder() {
  // definitions
  #define DELAY 100                               // number of polling-loops   
  static unsigned short integrator;               // encoder debounce
 
  /* the array values are determined by combining the previous and current A1,A0 
  Gray Code bit-patterns to form an array-index. +1 is assigned to all clockwise 
  (CW) indexes; -1 is assigned to all counter-clockwise (CCW) indexes; and 0 is 
  assigned to the remaining positions to indicate no movement*/   
  static char encoder[] = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0};
  static unsigned char encoder_index = 0;
  
  static long count = 0;                         // CW rotation = +1; CCW rotation = -1
  static long this_reading;
  static long last_reading;
  char value;                                    // return value

  /* check for encoder rotation. The absolute "count" value changes as the encoder rotates.
  Typically there are four states (counts) per click, but often more due to contact bounce.
  Bounce doesn't matter so long as the "count" decreases" for CCW rotation; remains constant 
  when there is no rotation; and increases with CW rotation. Bounce can be completely 
  elininated if we look for the "detent" code and integrate as shown below. */  
  encoder_index <<= 2;                           // shift previous bit-pattern left two places
  encoder_index |= (ENCODER_INPUT_PORT & 0x03);  // get current bit-pattern and combine with previous
  count += encoder[( encoder_index & 0x0f )];     
  
  // debounce encoder
  if ((ENCODER_INPUT_PORT & 0x03) == 3) {        // encoder on an indent (3 decimal == 00000011 binary)
    if (integrator < DELAY) {                    
      integrator++;
    } 
  } else {                                       // encoder not resting on indent
     if (integrator > 0) {
       integrator--;
     }
  }
  
  // prepare the "return" value
  if (integrator >= DELAY) {                     // encoder deemed to be stationary
    this_reading = count;
    if (this_reading < last_reading) value = -1;
    if (this_reading == last_reading) value = 0;
    if (this_reading > last_reading) value = 1;
    last_reading = this_reading;                 // both readings now hold current "count"
    integrator = 0;                              // reset integrator
  } else {
    value = 0;
  }
  return(value);
}
  
/******************************
 check frequency band
*******************************/
void check_band() {
  #define DELAY 100                             // number of polling-loops    
  static unsigned short integrator;             // encoder debounce
  
  static char encoder[] = {0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0};
  static unsigned char encoder_index = 0;
  static long count = 0;                        // CW rotation = +1; CCW rotation = -1
  static long this_reading;
  static long last_reading;  
   
  static long ham_band[] = {3500000,7000000,14000000,21000000};
  static signed char ham_band_index = 0;
   
  while (digitalRead(BAND_SWITCH) == LOW) { 
    // check for encoder rotation   
    encoder_index <<= 2; 
    encoder_index |= (ENCODER_INPUT_PORT & 0x03);
    count += encoder[( encoder_index & 0x0f )];     
    
    if ((ENCODER_INPUT_PORT & 0x03) == 3) {     // encoder on indent = B00000011
      if (integrator < DELAY) { 
        integrator++;
      }
    } else {                                    // encoder not resting on indent
      if (integrator > 0) {
	integrator--;
      }
    }
    
    if (integrator >= DELAY) {                  // encoder stationary
      this_reading = count;
      if (this_reading != last_reading) {
        if (this_reading > last_reading) {
          ham_band_index++;
          if (ham_band_index > 3) {
            ham_band_index =0;                  // wrap around
          }
        }
        if (this_reading < last_reading) {
          ham_band_index--;
          if (ham_band_index < 0) {
            ham_band_index = 3;
          }
        }
        last_reading = this_reading;              // both now hold current count
        integrator = 0;                           // reset integrator
 
        frequency = ham_band[ham_band_index];     // change frequency band  
        display_freq_step(); 
      }
    } 
  }
}

/******************************
 check the step size
*******************************/
void check_step_size() {
  // delay definitions
  #define DELAY 3000                             // DELAY = debounce(mS)*loops/second
  #define LONG_PUSH 15000                        // number of loops considered "long"
  
  // variables
  static boolean transition = false;             // valid during button push
  static unsigned short integrator;              // varies from 0 to MAXIMUM
  static unsigned short push_duration;           // determines step size/direction
  
  // look for first high-to-low transition
  if (!transition && (digitalRead(ENCODER_BUTTON) == LOW)) {
    transition = true;
    integrator = 0;
    push_duration = 0;
  }
  
  // look for last low-to-high transition
  if (transition) {
    if (push_duration < (LONG_PUSH*2)) {         // track push duration
      push_duration++;
    }
    if (digitalRead(ENCODER_BUTTON) == HIGH) {   // ENCODER_BUTTON is HIGH
      if (integrator < DELAY) {        
        integrator++;
      } else {                                   // ENCODER_BUTTON is LOW
        if (integrator > 0) {
	  integrator--;
	}
      }
    }
   
    // update the step_size
    if (integrator >= DELAY) { 
      transition = false;
      if (push_duration > LONG_PUSH) {          // LONG PUSH: decrease step size
        step_size = step_size/10;
        if (step_size < 10) {
          step_size = 1000000;                   // wrap around    
        }        
      } else {                                  // SHORT PUSH: increase step size
        step_size = step_size*10;
        if (step_size > 1000000) {
          step_size = 10;                       // wrap around
        }         
      }
      
      display_freq_step();                            // update step-size     
    }
  }
}

/********************************
 update frequency and step-size
********************************/
void display_freq_step() {
  long MHz = frequency/1000000;
   
  // convert frequency to floating point
  // adding 0.0000001 fixes a rounding error
  double freq = (double) frequency/1000000+0.0000001;

  // display frequency
  lcd.clear();
  if (MHz < 10) {
    lcd.setCursor(3,0);                          // start at char 1, line 1
    lcd.print(freq, 6);                          // 6 digit resolution
    lcd.print(" MHz ");
  } else {
    lcd.setCursor(2,0);                          // start at char 1, line 1
    lcd.print(freq, 6);
    lcd.print(" MHz ");
  }
  
  // last digit always zero                      // digit sometimes showed a 1
  lcd.setCursor(10,0);
  lcd.print("0");

  // display step size
  switch (step_size) {
    case 10:
      lcd.setCursor(9,1);
      lcd.print("*");       
      break;
    case 100:
      lcd.setCursor(8,1);
      lcd.print("*");       
      break;
    case 1000:
      lcd.setCursor(7,1);
      lcd.print("*");       
      break;
    case 10000:
      lcd.setCursor(6,1);
      lcd.print("*");       
      break;
    case 100000:
      lcd.setCursor(5,1);
      lcd.print("*");       
      break;      
    case 1000000:
      lcd.setCursor(3,1);                          // skip decimal point
      lcd.print("*");       
      break;

    default:
      break;
  }
}

