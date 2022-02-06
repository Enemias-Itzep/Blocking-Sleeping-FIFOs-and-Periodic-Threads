/*
 * threads.h
 *
 *  Created on: Mar 1, 2021
 *      Author: enemi
 */

#ifndef THREADS_H_
#define THREADS_H_

#include <G8RTOS.h>

//Defining MACROs for FIFOs
#define JOYSTICKFIFO 0
#define TEMPFIFO 1
#define LIGHTFIFO 2

//Semaphores used for LED and Sensor communication
extern semaphore_t sensorMutex; //used for sensor
extern semaphore_t LEDMutex; //used for displaying LEDs on board

//Background threads
void bThread0(void);
void bThread1(void);
void bThread2(void);
void bThread3(void);
void bThread4(void);
void bThread5(void);

//Periodic threads
void Pthread0(void);
void Pthread1(void);

#endif /* THREADS_H_ */
