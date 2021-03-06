#include "IMU.h"
#include "PID.h"
#include "RearMotor.h"
#include <math.h>

/*Define definite variables*/
//Front Motor
#define PWM_front 9
#define DIR 46
int steer_dir = 0;

//Rear Motor
#define PWM_rear 8  //rear motor PWM pin
#define hall_pin 11 //hall sgensor pulse 
#define reverse_pin 50 //to change direction of rear motor
//
//Rear Motor Variables
//float rearpwmmax = 150;sketch\._TinyGPS++.cpp:1:2: error: stray '\5' in program
float rear_pwm = 0; //current pwm value
double speed = 0; //speed in rev/s
boolean forward = true; //if False, is running in reverse
//Variables for calculating rear motor speed
float tOld = 0; //first time reading
float tNew = 0; //second time reading
double T = 0;

//Timed Loop Variables
const long interval = 10000;
long l_start;
long l_diff;

//Balance Control constants
const int k1 = 71; //phi = lean
const int k2 = 13; //was previously 21 //phidot=lean rate
const int k3 = -20; //delta=steer

//Encoder
const int quad_A = 2;
const int quad_B = 13;
const int idx = 60;
const unsigned int mask_quad_A = digitalPinToBitMask(quad_A);
const unsigned int mask_quad_B = digitalPinToBitMask(quad_B);
const unsigned int mask_idx = digitalPinToBitMask(idx);
int REnot = 3;
int DE = 4;
signed int oldPosition  = 0;
signed int oldIndex = 0;
unsigned long previous_t = 0;
signed int x_offset = 0;
float desired_pos = 0;
float current_pos = 0;
float current_vel = 0;
float desired_vel = 0;
float vel_error = 0;
float pos_error = 0;
float PID_output = 0;
float sp_error = 0;
float sv_error = 0;
int pwm = 0;

//count the number of times the time step has been calculated to calculate a running average time step
int numTimeSteps = 0;
float averageTimeStep = 0;
int n = 0;

float desired_steer = 0;
float desired_pos_array[250];
float theo_position = 0;

//Watchdog
#define WDI 42
#define EN 41

// LED
#define redLED 22
#define orangeLED 35
#define blueLED 36

//Landing Gear
#define relay1 48
#define relay2 47
#define relay4 49
float front_steer_value ;
float back_wheel_speed ;
float steer_contribution ;
float commanded_speed ;

//#define front_steer_value 51
//#define back_wheel_speed 28

#define relay3 50
#define relay4 49

//RC
#define RC_CH1 51     //Steer Angle 
#define RC_CH2 28     //
#define RC_CH3 25     //Velocity 
#define RC_CH4 33     //
#define RC_CH5 27     //Landing Gear
#define RC_CH6 32     //Kill Switch 

//timers for each channel
int duration_CH1, duration_CH2, duration_CH3, duration_CH4, duration_CH5, duration_CH6;
int start_CH1, start_CH2, start_CH3, start_CH4, start_CH5, start_CH6;
int end_CH1, end_CH2, end_CH3, end_CH4, end_CH5, end_CH6;
//current cycle's logic
boolean CH1, CH2, CH3, CH4, CH5, CH6;

//RC variables
float desired_angle;  //CH1
int PWM_rear_output;  //CH3


//voltage constants and variables
const int VOLTAGE_PIN = 63; //A9
float VOLTAGE_CONST = 14.2;
float battery_voltage = 0;
float VELOCITY_VOLTAGE_K = 1.7936;
float VELOCITY_VOLTAGE_C = -1.2002;

//define maximum front wheel pwm
int maxfront_PWM = 110;

//Read the relative position of the encoder
signed int relativePos = REG_TC0_CV0;
//Read the index value (Z channel) of the encoder
signed int indexValue = REG_TC0_CV1;


//set up timer for rc communication for steer and back motor speed
volatile unsigned long timer_start;  //micros when the pin goes HIGH
volatile unsigned long timer_start2;  //micros when the pin goes HIGH
volatile unsigned long timer_start6;  //micros when the pin goes HIGH
volatile int last_interrupt_time; //calcSignal is the interrupt handler
volatile int last_interrupt_time2; //calcSignal is the interrupt handler
volatile int last_interrupt_time6; //calcSignal is the interrupt handler
volatile float steer_range ;
volatile float forward_speed ;
volatile float pulse_time ;
volatile float pulse_time2 ;
volatile float pulse_time6 ;
//volatile float old_pulse_time6 = 1500 ; //value to store previous pulse length for comparrison, 1500 corresponds to RC_CH6 set to 1 (middle) which means do nothing in terms of landing gear
volatile float landing_gear_check = 0 ;

//difference between timer_start and micros() is the length of time that the pin
//was HIGH - the PWM pulse length. volatile int pulse_time;
//this is the time that the last interrupt occurred.
//you can use this to determine if your receiver has a signal or not.

void calcSignal()
{
  //record the interrupt time so that we can tell if the receiver has a signal from the transmitter
  last_interrupt_time = micros();
  //if the pin has gone HIGH, record the microseconds since the Arduino started up
  if (digitalRead(RC_CH1) == HIGH)
  {
    timer_start = micros();
  }
  //otherwise, the pin has gone LOW
  else
  {
    //only worry about this if the timer has actually started
    if (timer_start != 0 && ((volatile int)micros() - timer_start > 1100) && ((volatile int)micros() - timer_start < 1900) )
    {
      //record the pulse time
      pulse_time = ((volatile int)micros() - timer_start); //pulse time is the output from the rc value that we need to transform into a pwm value
      //restart the timer
      timer_start = 0;
    }
  }
}
void calcSignal2()
{
  //record the interrupt time so that we can tell if the receiver has a signal from the transmitter
  last_interrupt_time2 = micros();
  //if the pin has gone HIGH, record the microseconds since the Arduino started up
  if (digitalRead(RC_CH2) == HIGH)
  {
    timer_start2 = micros();
  }
  //otherwise, the pin has gone LOW
  else
  {
    //only worry about this if the timer has actually started
    if (timer_start2 != 0 && ((volatile int)micros() - timer_start2 > 1100) && ((volatile int)micros() - timer_start2 < 1900) )
    {
      //record the pulse time
      pulse_time2 = ((volatile int)micros() - timer_start2); //pulse time is the output from the rc value that we need to transform into a pwm value
      //restart the timer
      timer_start2 = 0;
    }
  }
}


void calcSignal6() //landing gear control
{


  //record the interrupt time so that we can tell if the receiver has a signal from the transmitter
  last_interrupt_time6 = micros();
  //if the pin has gone HIGH, record the microseconds since the Arduino started up
  if (digitalRead(RC_CH6) == HIGH)
  {
    timer_start6 = micros();
  }
  //otherwise, the pin has gone LOW
  else
  {
    //only worry about this if the timer has actually started
    if (timer_start6 != 0 && ((volatile int)micros() - timer_start6 > 1000) && ((volatile int)micros() - timer_start6 < 2000) ) // filter out noise
    {
      //record the pulse time
      pulse_time6 = ((volatile int)micros() - timer_start6); //pulse time is the output from the rc value that we need to transform into a pwm value
      //restart the timer
      timer_start6 = 0;

      //              //compare pulse_time6 to old_pulsetime6, if there is a significant difference RC_CH6 setting has changed. operate landing gear depending on RC_CH6 then store pulse_time6 into old_pulse_time6
      //              if (abs(old_pulse_time6 - pulse_time6) > 200){ //200 micro second set a tolerance for change in pulse time. theoritically the 3 possible settings are 1100, 1500, and 1900 (measured on oscilloscope)
      //                old_pulse_time6 = pulse_time6; //store current pulse_time6 into old_pulse_time6 for next iteration
      //
      //                // CH6 set to 2 on RC
      //                  if (pulse_time6 > 1000 && pulse_time6 < 1200){
      //                 //   if (forward_speed == 0){ //UNCOMMENT THIS SECTION TO MAKE LANDING GEAR DEPENDENT ON BACK MOTOR VELOCITY
      //                      landingGearDown();
      //                 //   }
      //                 //   else if (forward_speed > 100){
      //                 //     landingGearUp();
      //                 //   }
      //                  }
      //                   // CH6 set to 0 on RC
      //                  else if (pulse_time6 > 1800 && pulse_time6 < 2000){
      //                    landingGearUp();
      //                  }
      //
      //              }

    }
  }

}


//this is all normal arduino stuff
void setup()
{
  timer_start = 0;
  timer_start2 = 0;
  timer_start6 = 0;
  attachInterrupt(RC_CH1, calcSignal, CHANGE);
  attachInterrupt(RC_CH2, calcSignal2, CHANGE);
  attachInterrupt(RC_CH6, calcSignal6, CHANGE);

  Serial.begin(115200);
  initIMU();

  //setup rc
  //  pinMode(front_steer_value, INPUT);
  //  pinMode(back_wheel_speed, INPUT);

  //setup Encoder
  pinMode(REnot, OUTPUT);
  pinMode(DE, OUTPUT);
  // activate peripheral functions for quad pins
  REG_PIOB_PDR = mask_quad_A;     // activate peripheral function (disables all PIO functionality)
  REG_PIOB_ABSR |= mask_quad_A;   // choose peripheral option B
  REG_PIOB_PDR = mask_quad_B;     // activate peripheral function (disables all PIO functionality)
  REG_PIOB_ABSR |= mask_quad_B;   // choose peripheral option B
  REG_PIOB_PDR = mask_idx;     // activate peripheral function (disables all PIO functionality)
  REG_PIOB_ABSR |= mask_idx;   // choose peripheral option B


  // activate clock for TC0 and TC1
  REG_PMC_PCER0 = (1 << 27) | (1 << 28) | (1 << 29);

  // select XC0 as clock source and set capture mode
  REG_TC0_CMR0 = 5;


  // activate quadrature encoder and position measure mode, no filters
  REG_TC0_BMR = (1 << 9) | (1 << 8) | (1 << 12);


  // activate the interrupt enable register for index counts (stored in REG_TC0_CV1)
  REG_TC0_QIER = 1;


  // enable the clock (CLKEN=1) and reset the counter (SWTRG=1)
  // SWTRG = 1 necessary to start the clock!!
  REG_TC0_CCR0 = 5;
  REG_TC0_CCR1 = 5;

  //setup Motor Outputs
  pinMode(DIR, OUTPUT);
  pinMode (PWM_front, OUTPUT);
  pinMode (PWM_rear, OUTPUT);

  //setup Watchdog
  pinMode(WDI, OUTPUT);
  pinMode(EN, OUTPUT);
  digitalWrite(WDI, LOW);   // Doesn't matter if it is high or low, but initialize it
  digitalWrite(EN, LOW);

  //setup Landing Gear
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  //setup RC
  //pinMode(RC_CH1, INPUT);
  //pinMode(RC_CH2, INPUT);
  //pinMode(RC_CH3, INPUT);
  //pinMode(RC_CH4, INPUT);
  //pinMode(RC_CH5, INPUT);
  //pinMode(RC_CH6, INPUT);

  //setup LEDs
  pinMode(redLED, OUTPUT);
  pinMode(orangeLED, OUTPUT);
  pinMode(blueLED, OUTPUT);

  //setup Rear Motor
  pinMode(hall_pin, INPUT);
  //pinMode(PWM_rear, OUTPUT);
  pinMode(reverse_pin, OUTPUT);
  digitalWrite(reverse_pin, HIGH); //when high the motor goes forward
  float voltage = analogRead(63) / 14.2 * pwm / 180;
  analogWrite(PWM_rear, pwm);
  //attachInterrupt(digitalPinToInterrupt(hall_pin), getPeriod, RISING); //Interrupt //commented out for testing landing gear


  // initializes pins for the landing gear relay switch
  pinMode(48, OUTPUT);
  pinMode(47, OUTPUT);
  pinMode(49, OUTPUT);



  // tell user to press any key to calibrate wheel
  //  int  message_delivered = 0;
  //   while (!(Serial.available())) {
  //    if (message_delivered == 0) {
  //      Serial.println("Calibrate the front wheel") ;
  //      message_delivered = 1;
  //    }
  //   }
  //
  //  the follwing loop will not terminate until wheel passes front tick on encoder twice. The second time should be passed very slowly-
  //  this will allow for the most accurate location to be found for the center alignment of the front wheel with the bike.

  signed int y = REG_TC0_CV1;
  oldIndex = y;
  digitalWrite(DIR, HIGH);
  while (y == oldIndex) {
    analogWrite(PWM_front, 50);
    y = REG_TC0_CV1;
    //Serial.println("Ticking");
  }

  //set x offset to define where the front tick is with respect to the absolute position of the encoder A and B channels
  x_offset = REG_TC0_CV0;
  analogWrite(PWM_front, 0);
  //rear motor initialization

  int pwm = 60;
  int olav = 0;
  //  while (olav < pwm) { //Ramps up speed- Workaround for rear motor safety features
  //    analogWrite(PWM_rear, olav);
  //    delay(100);
  //    olav = olav + 10;
  //    //Serial.println("tteeeeeettt");
  //  }
  //  REG_TC0_CV0;
  //  delay(200) ;
  //  oldIndex = y;
  //  digitalWrite(DIR, LOW);
  //
  //  while(y==oldIndex){
  //    analogWrite(PWM_front,20);
  //    y = REG_TC0_CV1;
  //  }
  //
  //  Serial.println("TOCK");
  //  //redefine oldIndex to now be current y value
  //   oldIndex = y;
  //
  //   x_offset = REG_TC0_CV0;

  //  for (int i = 0; i< 250; i++){
  //    if (i< 50){
  //      desired_pos_array[i] = -(M_PI/2);
  //    }else if (i<100){
  //      desired_pos_array[i] = -(M_PI/4);
  //    }else if (i<150){
  //      desired_pos_array[i] = 0;
  //    }else if (i<200){
  //      desired_pos_array[i] = M_PI/4;
  //    }else{
  //      desired_pos_array[i] = M_PI/2;
  //    }
  //
  //  }
  rampToPWM(rear_pwm, 170);

}


//landing gear functions
void landingGearDown() {
  digitalWrite(48, LOW); //sets relay pin 1 to High (turns light on)
  digitalWrite(orangeLED, HIGH);
  digitalWrite(47, LOW); //sets relay pin 2 to High  (turns light on)
}
void landingGearUp() {
  digitalWrite(orangeLED, HIGH);
 // digitalWrite(49, LOW); //Let's break that circuit to give the relays time to switch HIGH MAKES CONNECTION
  digitalWrite(48, HIGH); //Sets relay pin 1 to low (turns light off)
  digitalWrite(47, HIGH); //Sets relay pin 2 to low (turns light off)
  digitalWrite(49,HIGH); //make connection
}

/* takes in desired angular velocity returns pwm */
int velocityToPWM (float desiredVelocity) {
  battery_voltage = analogRead(VOLTAGE_PIN);
  //Serial.println("pin 63 output " + String(battery_voltage));
  battery_voltage = battery_voltage / VOLTAGE_CONST;

  //Serial.println("voltage is " + String(battery_voltage));
  pwm = 256 * (desiredVelocity - VELOCITY_VOLTAGE_C) / (battery_voltage * VELOCITY_VOLTAGE_K);
  //Serial.println("pwm is  " + String(pwm));

  if (desiredVelocity > 18 ) { //***TO DO*** THIS LIMITATION MUST GO ON ALL OF THE PWM GOING TO THE FRONT MOTOR, NOT JUST THE FEED FORWARD LOOP
    //put in the warning
    return maxfront_PWM;
  } else {
    return pwm;
  }
}


/* intakes commanded velocity from balance controller
   converts commanded velocity into commanded position */
float eulerIntegrate(float desiredVelocity, float current_pos) {
  float desiredPosition = current_pos + desiredVelocity * ((float)interval / 1000000.0) ;
  return desiredPosition;
}


// updates global variables representing encoder position
float updateEncoderPosition() {
  //Read the relative position of the encoder
  relativePos = REG_TC0_CV0;
  //Read the index value (Z channel) of the encoder
  indexValue = REG_TC0_CV1;
  current_pos = (((relativePos - x_offset) * 0.02197 * M_PI) / 180); //Angle (rad)
  return current_pos;
}

/* takes in desired position and applies a PID controller to minimize error between current position and desired position */
void frontWheelControl(float desiredVelocity, float current_pos) { //steer contribution doese not need to be passed into
  //frontWheelControl because it is a global variable

  unsigned long current_t = micros();

  //  if (n == 0) {
  //    float desired_pos = 0;
  //    PID_Controller(desired_pos, relativePos, x_offset, current_t, previous_t, oldPosition);
  //    n++;
  //  }
  float desired_pos = eulerIntegrate(desiredVelocity, current_pos);
  //Serial.println(String(theo_position) + '\t' + String(desired_pos) + '\t' + String(current_pos)) ;

  /*
    if (Serial.available()){
    desired_pos = M_PI / 180 * Serial.parseFloat();
    }
  */



  //Serial.println(String(steer_contribution) + '\t' +  String(commanded_speed));

  PID_Controller(desired_pos, relativePos, x_offset, current_t, previous_t, oldPosition);

  previous_t = current_t;
  oldPosition = relativePos - x_offset;
}

/* FUNCTION THAT RETURNS DESIRED ANGULAR VELOCITY OF FRONT WHEEL */
float balanceController(float roll_angle, float roll_rate, float encoder_angle) {
  float desiredSteerRate = (k1 * roll_angle) + (k2 * roll_rate) + k3 * (encoder_angle - desired_steer);
  return desiredSteerRate;
}

struct roll_t {
  float rate;
  float angle;
};

// Retrieve data from IMU about roll angle and rate and return it
struct roll_t updateIMUData() {
  roll_t roll_data;
  //get data from IMU
  float roll_angle = getIMU(0x01);   //get roll angle
  float roll_rate = getIMU(0x26);    //get roll rate
  roll_data.angle = roll_angle;
  roll_data.rate = roll_rate;
  return roll_data;
}


//Loop variables
void loop() {

  l_start = micros();


  // First trial of rc stuff, trying to get steer angle in RC_CH1 to read in rc input
  int fromLow_forwardSpeed = 1100;
  int fromHigh_forwardSpeed = 1900;
  int toLow_forwardSpeed = 0;
  int toHigh_forwardSpeed = 200;
  forward_speed = map(pulse_time2, fromLow_forwardSpeed, fromHigh_forwardSpeed, toLow_forwardSpeed, toHigh_forwardSpeed);
  analogWrite(PWM_rear, forward_speed);

  int fromLow_steerRange = 1100;
  int fromHigh_steerRange = 1900;
  int toLow_steerRange = -70;
  int toHigh_steerRange = 70;
  steer_range = map(pulse_time, fromLow_steerRange, fromHigh_steerRange, toLow_steerRange, toHigh_steerRange);

  // Landing Gear Control logic
  if (pulse_time6 > 1000 && pulse_time6 < 1200 && landing_gear_check == 0) {
    landingGearDown();
    landing_gear_check = 1;

  }
  else if (pulse_time6 > 1800 && pulse_time6 < 2000 && landing_gear_check == 1) {
    landingGearUp();
    landing_gear_check = 0;
  }

  desired_steer = steer_range * .01 ;
  // desired_steer = steer_range*0.2 ;
  // Serial.println(String(pulse_time2) + '\t' + String(pulse_time) + '\t' + String(steer_range) + '\t' + String(desired_steer));
  // Serial.println(String(pulse_time2) + '\t' + String(forward_speed)  + '\t' + String(pulse_time6)  );

  // byte test_data = getIMU();
  // Serial.println(test_data);
  float encoder_position = updateEncoderPosition(); //output is current position wrt front zero

  roll_t imu_data = updateIMUData();
  //Serial.println(String(encoder_position) + '\t' + String(imu_data.angle) + '\t' + String(imu_data.rate)) ;
  // int imu_data_angle = 0; int imu_data_rate = 0; // ***IF YOU USE THIS LINE YOU MUST CHANGE THE FOLLOWING LINE TO imu_data_rate and ime_data_angle because those are the correct variable names
  float desiredVelocity = balanceController(((1) * (imu_data.angle)), (1) * imu_data.rate, encoder_position); //*****PUT IN OFFSET VALUE BECAUSE THE IMU IS READING AN ANGLE OFF BY +.16 RADIANS
  //float desiredVelocity = 0.0;

  //=============================Watchdog Implementation 1.0========================================
  if (fabs(encoder_position) > 0.7) {
    // Check if conditional is working with LED pins below
    digitalWrite(redLED, HIGH);
    digitalWrite(orangeLED,HIGH);
    digitalWrite(blueLED, HIGH);
    // Watchdog pins below
    // digitalWrite(EN,HIGH);
    // delayMicroseconds(200000);
  } else {
    digitalWrite(redLED, LOW);
    digitalWrite(orangeLED,LOW);
    digitalWrite(blueLED, LOW);
  }
  //============================finish watchdog stuff=================================================

  //===================================Sending data to Pi=============================================
  Serial.println(imu_data.angle + 1500);
//  Serial.println('\n');
  Serial.println(imu_data.rate + 2500);
//  Serial.print('\n');
  Serial.println(encoder_position + 3500);
//  Serial.print('\n');
  Serial.println(rear_pwm + 4500);
//  Serial.print('\n');
  Serial.println(l_start);
//  Serial.print('\n');
  //===================================End sending data to Pi=========================================

  frontWheelControl((-1)*desiredVelocity, encoder_position);  //DESIRED VELOCITY SET TO NEGATIVE TO MATCH SIGN CONVENTION BETWEEN BALANCE CONTROLLER AND


  //COMMENT OUT THE FOLLOWING PRINT STATMENTS WHEN DOING FULL TESTING, THOUGH THEY ARE NOT CALCULATED IN THE OUTPUTTED LOOP TIME (l_diff), they still will affect the PID function
  //Serial.println(String(imu_data.angle) + '\t' + String(imu_data.rate)  +  '\t' + String(encoder_position)/*  + '\t' + String(l_diff)*/) ;
  //Serial.println(l_diff);
  //    Serial.println(String(imu_data.angle) + '\t' + String(l_diff)) ;

  l_diff = micros() - l_start;   // Difference between final and initial loop times

  // Standardize the loop time by checking if it is currently less than the constant interval.
  // If it is, add the difference so the total loop time is standard.
  if (l_diff < interval) {
    delayMicroseconds(interval - l_diff);
    //Serial.println("LOOP TIME WAS: " + String(l_diff));
  } else {
    //Serial.println("LOOP LENGTH WAS VIOLATED. LOOP TIME WAS: " + String(l_diff));
  }
  /*
    Method that sets value "speed" to current speed in m/s
  */

}
void getPeriod() {
  float tOld = tNew;
  tNew = micros();
  double T = (tNew - tOld);
  double speed = (1.2446) * (1E6) / (28 * T) ;
  if (speed < 100) {
    // Serial.println(speed, 3);
  }
}

