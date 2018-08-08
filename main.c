
/**
 * Example application demonstrating task procedure sharing, message passing and more.
 *
 * The application consists of four tasks:
 *
 *  - temp sensor task: waits for an event to signaled from the temp sensor driver. New temp
 *    data is fetched from the sensor and sent in a message to the display task.
 *
 *  - gyro sensor task: waits for a timeout and then polls the sensor for new data. New data is
 *    fetched from the sensor and sent in a message to the display task.
 *
 *  - control task: waits for up/down arrow events to be signaled and then changes channel of
 *    the temp sensor.
 *
 *  - display task: writes sensor values to the terminal
 *
 *  Two linux threads are created at startup:
 *   - One that simulates a timer tick and calls os_tick(). It also periodically services the
 *     sensors. This makes the sensors signal it has new data by signaling an event or setting
 *     a polled flag.
 *
 *   - One thread that reads characters from stdin and signals cocoOS events when up/down arrow are pressed.
 *
 *  Main flow of execution:
 *  When the sensors are serviced they eventually signals new data available by signaling an event (temp
 *  sensor) or sets a polled flag (gyro sensor). The sensor tasks waiting for event/timeout, checks the
 *  sensor for new data and posts a message with the new data to the display task.
 *
 *  Simultaneously, the thread reading characters from stdin, signals events when up/down
 *  arrow keys are pressed on the keyboard. The control task then changes channel on the temp sensor.
 *
 *  Task procedure sharing
 *  The two sensor tasks use the same task procedure. All work and data handling is done through the task
 *  data pointer assigned to each task. This points to a structure holding sensor configuration/functions
 *  and an array holding sensor data.
 *
 */
#include <Arduino.h> ////
#include <Time.h> ////
#include <TimeLib.h> ////
typedef unsigned long time_t; ////  TODO: Fix the declaration

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <cocoos-cpp.h>  //  TODO: Workaround for cocoOS in C++
#include "sensor.h"
#include "temp_sensor.h"
#include "gyro_sensor.h"
#include "display.h"

//  Global semaphore for preventing concurrent access to the single shared I2C Bus on Arduino Uno.
Sem_t i2cSemaphore;  //  Declared in sensor.h

//  Events that will be raised by sensors.
static Evt_t tempEvt;
static Evt_t prevChEvt;
static Evt_t nextChEvt;

//  Task Data used by the sensor tasks to remember the sensor context.
static SensorTaskData tempSensorTaskData;
static SensorTaskData gyroSensorTaskData;

/********************************************/
/*            System threads                */
/********************************************/

static void arduino_start_timer(void) { ////
  //  Start the AVR Timer 1 to generate interrupt ticks for cocoOS to perform
  //  background processing.  AVR Timer 0 is reserved for Arduino timekeeping.
  
  // Set PORTB pins as output, but off
	DDRB = 0xFF;
	PORTB = 0x00;

	// Turn on timer 
	// TCCR1B |= _BV(CS10);  // no prescaler
	TCCR1B = (1<<CS10) | (1<<CS12); //set the prescaler as 1024
	TIMSK1 |= _BV(TOIE1);

	// Turn interrupts on.
	sei();	
} ////

ISR(TIMER1_OVF_vect) { ////
  //  Handle the AVR Timer 1 interrupt. Trigger an os_tick() for cocoOS to perform background processing.
  // debug("os_tick"); ////
  os_tick();  
} ////

/********************************************/
/*            Setup and main                */
/********************************************/

static void arduino_setup(void) { ////
  //  Run initialisation for Arduino, since we are using main() instead of setup()+loop().
  init();  // initialize Arduino timers  
  debug("----arduino_setup", 0);
} ////

static void system_setup(void) {
  arduino_setup(); ////
  debug("init_display", 0); ////
  init_display();

  //  Create the global semaphore for preventing concurrent access to the single shared I2C Bus on Arduino Uno.
  debug("Create semaphore", 0); ////
  const int maxCount = 10;  //  Allow up to 10 tasks to queue for access to the I2C Bus.
  const int initValue = 1;  //  Allow only 1 concurrent access to the I2C Bus.
  i2cSemaphore = sem_counting_create( maxCount, initValue );
}

static void sensor_setup(uint8_t display_task_id) {
  //  Create the events that will be raised by the sensors.
  //// debug("event_create", 0); ////
  tempEvt   = event_create();
  prevChEvt = event_create();
  nextChEvt = event_create();

  //  Initialize the sensors.
  //// debug("get_temp_sensor"); ////
  const int pollIntervalMillisec = 500;  //  Poll the sensor every 500 milliseconds.
  tempSensorTaskData.display_task_id = display_task_id;
  tempSensorTaskData.sensor = get_temp_sensor();
  tempSensorTaskData.sensor->control.init_sensor_func(TEMP_DATA, &tempEvt, pollIntervalMillisec);

  //// debug("get_gyro_sensor"); ////
  gyroSensorTaskData.display_task_id = display_task_id;
  gyroSensorTaskData.sensor = get_gyro_sensor();
  gyroSensorTaskData.sensor->control.init_sensor_func(GYRO_DATA, 0, pollIntervalMillisec);

  //  Create 2 sensor tasks using same task function, but with unique task data.
  //  "0, 0, 0" means that the tasks may not receive any message queue data.
  //// debug("task_create"); ////
  task_create(sensor_task, &tempSensorTaskData, 10,  //  Priority 10 = highest priority
    0, 0, 0);  //  Will not receive message queue data.
  task_create(sensor_task, &gyroSensorTaskData, 20,  //  Priority 20
    0, 0, 0);  //  Will not receive message queue data.
}

//  Pool of display messages that make up the display message queue.
#define displayMsgPoolSize 5
static DisplayMsg displayMsgPool[displayMsgPoolSize];

int main(void) {
  //  Init the system and OS for cocoOS.
  system_setup();
  os_init();

  //  Start the display task that displays sensor readings
  uint8_t display_task_id = task_create(
    display_task,  //  Task will run this function.
    get_display(),  //  task_get_data() will be set to the display object.
    100,  //  Priority 100 = lowest priority
    (Msg_t *) displayMsgPool,  //  Pool to be used for storing the queue of display messages.
    displayMsgPoolSize,  //  Size of queue pool.
    sizeof(DisplayMsg));  //  Size of queue message.
  
  //  Setup the sensors for reading.
  sensor_setup(display_task_id);

  //  Start the AVR timer to generate ticks for background processing.
  //// debug("arduino_start_timer"); ////
  arduino_start_timer(); ////

  //  Start cocoOS task scheduler, which runs the sensor tasks and display task.
  //// debug("os_start"); ////
  os_start();  //  Never returns.
  
	return EXIT_SUCCESS;
}
