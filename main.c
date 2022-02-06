#include "msp.h"
#include <driverlib.h>
#include "BSP.h"
#include "G8RTOS.h"
#include "threads.h"

/**
 * main.c
 */
/* Configuration for UART */
static const eUSCI_UART_Config Uart115200Config =
{
    EUSCI_A_UART_CLOCKSOURCE_SMCLK, //SMCLK Clock Source
    6, // BRDIV
    8, // UCxBRF
    0, // UCxBRS
    EUSCI_A_UART_NO_PARITY, // No Parity
    EUSCI_A_UART_LSB_FIRST, //LSB First
    EUSCI_A_UART_ONE_STOP_BIT, // One stop bit
    EUSCI_A_UART_MODE, // UART mode
    EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION // Oversampling
};

/*
 * Initializes the USART
 */
void uartInit()
{
    /* select the GPIO functionality */
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1, GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

    /* configure the digital oscillator */
    CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_12);

    /* configure the UART with baud rate 115200 */
    MAP_UART_initModule(EUSCI_A0_BASE, &Uart115200Config);

    /* enable the UART */
    MAP_UART_enableModule(EUSCI_A0_BASE);
}

void main(void)
{
    //Initialize G8RTOS
    G8RTOS_Init();

    //Initializes UART
    uartInit();

    //Initializing Semaphores
    G8RTOS_InitSemaphore(&LEDMutex, 1);
    G8RTOS_InitSemaphore(&sensorMutex, 1);

    //Adding background thread to scheduler

    //Reading temperature sensor
    while(!(G8RTOS_AddThread(&bThread0) + 1));

    //Read light sensor send through FIFO
    while(!(G8RTOS_AddThread(&bThread1) + 1));

    //Read FIFO, calculate RMS, set global
    while(!(G8RTOS_AddThread(&bThread2) + 1));

    //Reading from TEMPFIFO and displaying on LED
    while(!(G8RTOS_AddThread(&bThread3) + 1));

    //Read Joy FIFO and output
    while(!(G8RTOS_AddThread(&bThread4) + 1));

    //Empty loop
    while(!(G8RTOS_AddThread(&bThread5) + 1));

    //Adding periodic thread to scheduler
    //Joystick
    while(!(G8RTOS_AddPeriodicEvent(&Pthread0, 100) + 1));


    while(!(G8RTOS_AddPeriodicEvent(&Pthread1, 1000) + 1));

    //Create FIFOs
    while(!(G8RTOS_InitFIFO(JOYSTICKFIFO) + 1));
    while(!(G8RTOS_InitFIFO(TEMPFIFO) + 1));
    while(!(G8RTOS_InitFIFO(LIGHTFIFO) + 1 ));

    //Initialize GPIO pints at outputs
    //Configures the GPIO pins 3.5 3.7 5.1
    P3->DIR |= BIT5;
    P2->DIR |= BIT3;
    P5->DIR |= BIT1;
    BITBAND_PERI(P2->OUT,3) = 0;
    BITBAND_PERI(P3->OUT,5) = 0;
    BITBAND_PERI(P5->OUT,1) = 0;

    //Start GatorOS
    G8RTOS_Launch();
}
