/*  Westerstrand clock driver
    (c) A.G.Doswell 2018

    PWM referenced clock, driving a Westerstrand electro-mechanical clock
    Has remote 433 MHz receiver for reception of remote time sync signal from master clock project
    (http://andydoz.blogspot.com/2016/07/arduino-gps-master-clock-with-433315.html)

    See https://andydoz.blogspot.com/2018/05/synchronised-westerstrand-impulse-clock.html
    for more information and the circuit diagram.

*/
int clockInt = 0;            // Digital pin 2 is now interrupt 0
int masterClock = 0;         // Counts rising edge clock signals
int seconds = 0;             // Seconds
int minutes = 0;            // Minutes
int hours = 0;              // Hours
int positivePulsePin = 5;  // Output pin for the h-bridge for +ve pulse
int negativePulsePin = 6;  // .. and -ve pulse
int hourPin = 7;          // Hour set switch
int minPin = 8;           // Min set switch
int displayMinutes;            // Actual minutes displayed
boolean clockSetFlag = false; // True when clock has been set for the first time.
int totalMinutes;
const int delaymS = 100;
boolean pulseDirection;

// set up for remote master clock
#include <VirtualWire.h> // available from https://www.pjrc.com/teensy/td_libs_VirtualWire.html
#include <VirtualWire_Config.h>
int rxTX_ID; //rx indicates remotely received data
int rxHours;
int rxMins;
int rxSecs;
int rxDay;
int rxMonth;
int rxYear;
char buffer[5];

void setup() {
  Serial.begin(57600);
  pinMode (positivePulsePin, OUTPUT);
  pinMode (negativePulsePin, OUTPUT);
  pinMode (hourPin, INPUT_PULLUP);  // cal switch connected to pin 7. This forwards the clock 1 hour
  pinMode (minPin, INPUT_PULLUP);  // Moves the clock movement forward 1 min
  analogWrite(11, 127);     // This starts our PWM 'clock' ticking with a 490 Hz 50% duty cycle
  vw_set_tx_pin(12); // Even though it's not used, we'll define it.
  vw_set_rx_pin(13); // RX pin set to pin 13
  vw_setup(1200); // Sets virtualwire for a tx rate of 1200 bits per second
  vw_rx_start(); // Start the receiver
}

void loop() {
  remoteClockSet (); // check to see if the clock has a sync message waiting
  if (!clockSetFlag) { // if the clock has not been synced...
    //detachInterrupt (clockInt);   // Stop the interrupt clock running
    if (digitalRead (hourPin) == false) { // advance one hour
      for (int i = 0; i <= 59; i++) {
        pulse ();
      }
    }
    if (digitalRead (minPin) == false) { //advance one minute
      pulse ();
    }
    displayMinutes = 0; // set reference of 12 o'clock
  }
  if (clockSetFlag) {
    if (seconds >= 60) {   // Put down your knitting grandma, the real timekeeping starts here
      minutes ++;          // Increment minutes by 1
      seconds = 0;         // Reset the seconds
    }
    if (minutes >= 60) {
      hours ++;             // Increment hours
      minutes = 0;          // Reset minutes
    }
    if (hours >= 12) {
      hours = 0;
    }
    totalMinutes = (hours * 60) + minutes;
    if (displayMinutes >= 720)
      displayMinutes = 0;
    if (totalMinutes != displayMinutes) {
      pulse();
    }
  }
}

void clockCounter()        // Called by interrupt, driven by the PWM
{
  masterClock ++;          // With each clock rise add 1 to masterClock count
  if (masterClock >= 490)  // 490Hz reached
  {
    seconds ++;            // Add 1 second
    masterClock = 0;       // Reset
  }
  return;
}

void remoteClockSet () { // Remotely sets the clock
  typedef struct rxRemoteData //Defines the received data, this is the same as the TX struct
  {
    int rxTX_ID;
    int rxHours;
    int rxMins;
    int rxSecs;
    int rxDay;
    int rxMonth;
    int rxYear;
  };
  struct rxRemoteData receivedData;
  uint8_t rcvdSize = sizeof(receivedData);
  if (vw_get_message((uint8_t *)&receivedData, &rcvdSize)) // Process message if received
  {
    if (receivedData.rxTX_ID == 1)     { //Only if the TX_ID=1 do we process the data.
      rxTX_ID = receivedData.rxTX_ID;
      rxHours = receivedData.rxHours;
      rxMins = receivedData.rxMins;
      rxSecs = receivedData.rxSecs;
      rxDay =  receivedData.rxDay;
      rxMonth =  receivedData.rxMonth;
      rxYear =  receivedData.rxYear;
      // Set the clock
      hours = rxHours;
      if (isBST())  // Is it British Summer Time?
        hours++;
      if (hours >= 12)  // 0-12 hours only...
        hours = hours - 12;
      minutes = rxMins;
      seconds = rxSecs;
      masterClock = 0;
      attachInterrupt(clockInt, clockCounter, RISING); // set the interrupt clock running
      clockSetFlag = true; // set the clockSetFlag, this disables the adjustment mode, and starts the clock running
    }
  }
}

boolean isBST() { // this bit of code blatantly plagiarised from http://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html
  //January, february, and november are out.
  if (rxMonth < 3 || rxMonth > 10)
    return false;
  //April to September are in
  if (rxMonth > 3 && rxMonth < 10)
    return true;
  // find last sun in mar and oct - quickest way I've found to do it
  // last sunday of march
  int lastMarSunday =  (31 - (5 * rxYear / 4 + 4) % 7);
  //last sunday of october
  int lastOctSunday = (31 - (5 * rxYear / 4 + 1) % 7);

  //In march, we are BST if is the last sunday in the month
  if (rxMonth == 3) {

    if ( rxDay > lastMarSunday)
      return true;
    if ( rxDay < lastMarSunday)
      return false;
    if (rxHours < 1)
      return false;
    return true;
  }
  //In October we must be before the last sunday to be bst.
  //That means the previous sunday must be before the 1st.
  if (rxMonth == 10) {
    if ( rxDay < lastOctSunday)
      return true;
    if ( rxDay > lastOctSunday)
      return false;
    if (rxHours >= 1)
      return false;
    return true;
  }
}

void pulse () { // pulse the clock movement to advance one minute
  if (pulseDirection) {
    digitalWrite (positivePulsePin, HIGH);
    delay (delaymS);
    digitalWrite (positivePulsePin, LOW);
    delay (delaymS);
    pulseDirection = !pulseDirection;
  }
  else {
    digitalWrite (negativePulsePin, HIGH);
    delay (delaymS);
    digitalWrite (negativePulsePin, LOW);
    delay (delaymS);
    pulseDirection = !pulseDirection;
  }
  displayMinutes ++; // increment the minute counter
}



