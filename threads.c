/*
 * threads.c
 *
 *  Created on: Mar 1, 2021
 *      Author: enemi
 */
#include "threads.h"
#include <BSP.h>
#include <stdlib.h>
#include <stdio.h>
#include <driverlib.h>
#include "G8RTOS_Semaphores.h"
#include "G8RTOS_IPC.h"

//Global for local light array size
#define lSize 8

//Holds decayed average value
int32_t avg = 0;

//Indicates if RMS is less than 5000
uint32_t lightGlobal = 0;

//Reads light FIFO
uint32_t temperature = 0;

/* method to transmit a string through USART */
static inline void uartTransmitString(char * s)
{
    /* Loop while not null */
    while(*s)
    {
        MAP_UART_transmitData(EUSCI_A0_BASE, *s++);
    }
}

/*
 * Function that finds the square root of n
 */
static void rootMeanSquare(uint64_t n)
{
    uint64_t xk1 = 0;
    uint64_t xk = n;

    //Newtons method to finding a square root
    while(true)
    {
        xk1 = (xk + (n / xk)) / 2;

        if(abs(xk1-xk) < 1)
        {
            break;
        }
        xk = xk1;
    }

    //If Xrms is lower than 5000, set global to true
    if(xk1 < 5000)
    {
        lightGlobal = 1;
    }
    else
    {
        lightGlobal = 0;
    }
}


/*
 * a. Read the BME280’s temperature sensor
    b. Send data to temperature FIFO
    c. Toggle an available GPIO pin (don’t forget
    to initialize it in your main)
    d. G8RTOS_Sleep for 500ms
 */
void bThread0(void)
{
    while(1)
    {
        //Holds uncompressed data
        int32_t data;

        //Waits for sensor I2C semaphore
        G8RTOS_WaitSemaphore(&sensorMutex);

        while(bme280_read_uncomp_temperature(&data));

        //Releases Sensor
        G8RTOS_SignalSemaphore(&sensorMutex);

        //Interprets uncompressed data
        int32_t temperature = bme280_compensate_temperature_int32(data);

        int status = writeFIFO(TEMPFIFO, temperature/100);

        //Toggle GPIO pin P5.1
        BITBAND_PERI(P5->OUT,1) = ~((P5->OUT & BIT1) >> 1);

        //Places thread to Sleep for 500ms
        G8RTOS_Sleep(500);
    }
}

/*
 * a. Read light sensor
    b. Send data to light FIFO
    c. Toggle an available GPIO pin (don’t forget
    to initialize it in your main)
    d. G8RTOS_Sleep for 200ms
 */
void bThread1(void)
{
    while(1)
    {
        //Reads accelerometer
        uint16_t light;

        //Waits for sensor I2C semaphore
        G8RTOS_WaitSemaphore(&sensorMutex);

        while(!sensorOpt3001Read(&light));

        //Releases Sensor
        G8RTOS_SignalSemaphore(&sensorMutex);

        //Sends light data to FIFO
        int status = writeFIFO(LIGHTFIFO, light);

        //Toggle GPIO pin P2.3
        BITBAND_PERI(P2->OUT,3) = ~((P2->OUT & BIT3) >> 3);

        //Sleep for 200ms
        G8RTOS_Sleep(200);
    }
}
/*
 * a. Read light FIFO
    b. Calculate RMS value (See appendix for
    details)
    c. You should write a static function to
    calculate the square root of a value using
    Newton’s method
    d. If RMS < 5000, set global variable to true,
    otherwise keep it false
 */
void bThread2(void)
{
    //Array that holds past values
    uint64_t lightArray [lSize] = {0};
    uint8_t index = 0;
    while(1)
    {
        //Reads light FIFO
        uint32_t light = readFIFO(LIGHTFIFO);

        //Checks if index is greater than size, then
        if(index >= lSize)
        {
            index = 0;
        }

        //Saves data into index and increments
        lightArray[index] = light;
        index++;

        //Holds value inside square root
        uint64_t n = 0;
        int i;
        for (i = 0; i < lSize; i++)
        {
            n += (lightArray[i]*lightArray[i]);
        }

        n /= lSize; //FIX

        //Calculates RMS value and sets global
        rootMeanSquare(n);

    }
}
/*
 * a. Read temperature FIFO
    b. Output data to LEDs as shown in Figure B
 */
void bThread3(void)
{
    //Values that hold the LED output
    uint16_t blueLED, redLED = 0x0000;

    while(1)
    {
        //Reads light FIFO
        temperature = readFIFO(TEMPFIFO);
        //Converts to Fahrenheit
        temperature = ((temperature * 9) / 5) + 32;

        //Calculates what to output to LEDs
        if(temperature > 84)
        {
            blueLED = 0x0000;
            redLED = 0x00FF;
        }
        else if(temperature > 81)
        {
            blueLED = 0x0080;
            redLED = 0x007F;
        }
        else if(temperature > 78)
        {
            blueLED = 0x00C0;
            redLED = 0x003F;
        }
        else if(temperature > 75)
        {
            blueLED = 0x00E0;
            redLED = 0x001F;
        }
        else if(temperature > 72)
        {
            blueLED = 0x00F0;
            redLED = 0x000F;
        }
        else if(temperature > 69)
        {
            blueLED = 0x00F8;
            redLED = 0x0007;
        }
        else if(temperature > 66)
        {
            blueLED = 0x00FC;
            redLED = 0x0003;
        }
        else if(temperature > 63)
        {
            blueLED = 0x00FE;
            redLED = 0x0001;
        }
        else if(temperature > 60)
        {
            blueLED = 0x00FF;
            redLED = 0x0000;
        }

        //Waits for LED I2C semaphore
        G8RTOS_WaitSemaphore(&LEDMutex);

        //Output values on LEDs
        LP3943_LedModeSet(BLUE, blueLED);
        LP3943_LedModeSet(RED,  redLED);

        //Releases Sensor
        G8RTOS_SignalSemaphore(&LEDMutex);
    }
}

/*
 * a. Read Joystick FIFO
    b. Calculate decayed average for X-Coordinate
    (See appendix for details)
    c. Output data to LEDs as shown in Figure A
 */
void bThread4(void)
{
    uint32_t greenLED = 0x0000;

    while(1)
    {
        //Reads joystick FIFO
        int32_t data = readFIFO(JOYSTICKFIFO);

        //Calculates decayed average value for coordinate X
        avg = (avg + data) >> 1;

        if (avg > 6000)
        {
            greenLED = 0xF000;
        }
        else if (avg > 4000)
        {
            greenLED = 0x7000;
        }
        else if (avg > 2000)
        {
            greenLED = 0x3000;
        }
        else if (avg > 500)
        {
            greenLED = 0x1000;
        }
        else if (avg > -500)
        {
            greenLED = 0x0000;
        }
        else if (avg > -2000)
        {
            greenLED = 0x0800;
        }
        else if (avg > -4000)
        {
            greenLED = 0x0C00;
        }
        else if (avg > -6000)
        {
            greenLED = 0x0E00;
        }
        else if (avg > -8000)
        {
            greenLED = 0x0F00;
        }

        //Waits for LED I2C semaphore
        G8RTOS_WaitSemaphore(&LEDMutex);

        //Output the value on the green LED
        LP3943_LedModeSet(GREEN, greenLED);

        //Releases Sensor
        G8RTOS_SignalSemaphore(&LEDMutex);
    }
}
/*
 *
 */
void bThread5(void)
{
    while(1);
}

/* 100ms
 * a. Read X-coordinate from joystick
    b. Write data to Joystick FIFO
    c. Toggle an available GPIO pin (don’t forget
    to initialize it in your main)
 */
void Pthread0(void)
{
    //Variables that will hold coordinates
    int16_t x, y;

    //Waits for sensor I2C semaphore
    //G8RTOS_WaitSemaphore(&sensorMutex);

    //Gets Joystick coordinates
    GetJoystickCoordinates(&x, &y);

    //Releases Sensor
    //G8RTOS_SignalSemaphore(&sensorMutex);

    //Sends joystick data to FIFO
    int status = writeFIFO(JOYSTICKFIFO, x);

    //Toggle GPIO pin P3.5
    BITBAND_PERI(P3->OUT,5) = ~((P3->OUT & BIT5) >> 5);
}

/* 1s
 * a. If global variable for light sensor is true, do
    b & c; otherwise, do nothing
    b. Print out the temperature (in degrees
    Fahrenheit) via UART
    c. Print out decayed average value of the
    Joystick’s X-coordinate via UART
 */
void Pthread1(void)
{
    if(lightGlobal)
    {
        //Reads light FIFO and calculates temperature in Farenheit
        char str1[255];
        char str2[255];

        //Creates strings to print out
        snprintf(str1, 255, "Temperature in Fahrenheit is: %d", temperature);
        snprintf(str2, 255, "Decayed average value is: %d", avg);

        //Transmits through Temperature
        uartTransmitString(str1);

        //New Line
        char temp = 10;
        uartTransmitString(&temp);
        temp = 13;
        uartTransmitString(&temp);

        //Transmits decayed average value
        uartTransmitString(str2);

        //New Line
        temp = 10;
        uartTransmitString(&temp);
        temp = 13;
        uartTransmitString(&temp);
    }
    return;
}
