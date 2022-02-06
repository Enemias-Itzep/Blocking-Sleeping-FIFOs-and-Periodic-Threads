/*
 * G8RTOS_Scheduler.c
 */

/*********************************************** Dependencies and Externs *************************************************************/

#include <stdint.h>
#include "msp.h"
#include "BSP.h"
#include "G8RTOS_Scheduler.h"
#include "G8RTOS.h"
#include <stdint.h>
/*
 * G8RTOS_Start exists in asm
 */
extern void G8RTOS_Start();

/* System Core Clock From system_msp432p401r.c */
extern uint32_t SystemCoreClock;

/*
 * Pointer to the currently running Thread Control Block
 */
extern tcb_t * CurrentlyRunningThread;

/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Defines ******************************************************************************/

/* Status Register with the Thumb-bit Set */
#define THUMBBIT 0x01000000

/*********************************************** Defines ******************************************************************************/


/*********************************************** Data Structures Used *****************************************************************/

/* Thread Control Blocks
 *	- An array of thread control blocks to hold pertinent information for each thread
 */
static tcb_t threadControlBlocks[MAX_THREADS];

/* Thread Stacks
 *	- An array of arrays that will act as individual stacks for each thread
 */
static int32_t threadStacks[MAX_THREADS][STACKSIZE];

/* Periodic Event Threads
 * - An array of periodic events to hold pertinent information for each thread
 */
static ptcb_t Pthread[MAXPTHREADS];

/*********************************************** Data Structures Used *****************************************************************/


/*********************************************** Private Variables ********************************************************************/

/*
 * Current Number of Threads currently in the scheduler
 */
static uint32_t NumberOfThreads;

/*
 * Current Number of Periodic Threads currently in the scheduler
 */
static uint32_t NumberOfPthreads;

/*********************************************** Private Variables ********************************************************************/


/*********************************************** Private Functions ********************************************************************/

/*
 * Initializes the Systick and Systick Interrupt
 * The Systick interrupt will be responsible for starting a context switch between threads
 * Param "numCycles": Number of cycles for each systick interrupt
 */
static void InitSysTick(uint32_t numCycles)
{
    SysTick_Config(numCycles); //Configures SysTick overflow by amount of cycles
}

/*
 * Chooses the next thread to run.
 * Lab 2 Scheduling Algorithm:
 * 	- Simple Round Robin: Choose the next running thread by selecting the currently running thread's next pointer
 * 	- Check for sleeping and blocked threads
 */
void G8RTOS_Scheduler()
{
    while(1)
    {
        //Changes currently running thread to next
        CurrentlyRunningThread = CurrentlyRunningThread->next;

        //If currently running thread is blocked OR asleep, next one is used
        if(CurrentlyRunningThread->blocked || CurrentlyRunningThread->asleep)
        {
            continue;
        }
        //If not blocked or asleep, return
        return;
    }
}


/*
 * SysTick Handler
 * The Systick Handler now will increment the system time,
 * set the PendSV flag to start the scheduler,
 * and be responsible for handling sleeping and periodic threads
 */
void SysTick_Handler()
{
    //Increments system time
    SystemTime++;

    //Checks periodic threads to see if they are to be dealt with
    for(uint8_t i = 0; i < NumberOfPthreads; ++i)
    {
        //If execute time is equal to system time
        if(Pthread[i].executeTime <= SystemTime)
        {
            //Defines new execute time to be "period" ms from now
            Pthread[i].executeTime = Pthread[i].period + SystemTime + Pthread[i].currentTime;

            //Runs Periodic thread
            (*(Pthread[i].Handler))();
        }
    }

    //Wakes up threads that need to be woken up
    tcb_t *temp = CurrentlyRunningThread;
    for(uint8_t i = 0; i < NumberOfThreads; ++i)
    {
        //If thread is asleep
        if(temp->asleep)
        {
            //If thread reaches wake up time, then wake it up threadControlBlocks[i].sleepCount == SystemTime
            if(temp->sleepCount == SystemTime)
            {
                //Thread woken up and sleep count made 0
                temp->asleep = false;
                temp->sleepCount = 0;
            }
        }
        //Points to the next thread
        temp = temp->next;
    }

    //Sets PendSV flag
    SCB->ICSR |= (1<<28);
}

/*********************************************** Private Functions ********************************************************************/


/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Sets variables to an initial state (system time and number of threads)
 * Enables board for highest speed clock and disables watchdog
 */
void G8RTOS_Init()
{
    //Sets system time to 0
    SystemTime = 0;

    //Sets number of threads to 0
    NumberOfThreads = 0;

    //Sets number of periodic threads to 0
    NumberOfPthreads = 0;

    //Initializes board
    BSP_InitBoard();
}

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes the Systick
 * 	- Sets Context to first thread
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int G8RTOS_Launch()
{
    //Sets currently running thread to first tcb
    CurrentlyRunningThread = &threadControlBlocks[0];

    //Gets clock frequency
    uint32_t clkFreq = ClockSys_GetSysFreq();

    //Initializes SysTick
    InitSysTick((clkFreq/1000));

    //Sets priorities for PENDSV and SysTick
    NVIC_SetPriority(PendSV_IRQn, 7);

    //Call G8RTOS_Start
    G8RTOS_Start();

    return ERROR;
}


/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are stil available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread to hold a "fake context"
 * 	- Sets stack tcb stack pointer to top of thread stack
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Returns: Error code for adding threads
 */
int G8RTOS_AddThread(void (*threadToAdd)(void))
{
    //If maximum threads have not been reached
    if(MAX_THREADS != NumberOfThreads)
    {
        //When no threads exist
        if (!NumberOfThreads)
        {
            //Makes new thread point to itself
            threadControlBlocks[NumberOfThreads].prev = &threadControlBlocks[NumberOfThreads];
            threadControlBlocks[NumberOfThreads].next = &threadControlBlocks[NumberOfThreads];
        }
        else
        {
            //Makes new thread point to previous and head
            threadControlBlocks[NumberOfThreads].prev = &threadControlBlocks[NumberOfThreads - 1];
            threadControlBlocks[NumberOfThreads].next = &threadControlBlocks[0];

            //Makes previous root and head point to new thread
            threadControlBlocks[NumberOfThreads - 1].next = &threadControlBlocks[NumberOfThreads];
            threadControlBlocks[0].prev = &threadControlBlocks[NumberOfThreads];
        }
        //Makes thread start awake
        threadControlBlocks[NumberOfThreads].asleep = 0;

        //Makes thread sleep count equal 0
        threadControlBlocks[NumberOfThreads].sleepCount = 0;

        //Makes blocked semaphore 0
        threadControlBlocks[NumberOfThreads].blocked = 0;


        //Sets thumbbit in xPSR
        threadStacks[NumberOfThreads][STACKSIZE - 1] = THUMBBIT;

        //Sets PC to function pointer
        threadStacks[NumberOfThreads][STACKSIZE - 2] = (uint32_t)threadToAdd;

        //Fills R0 to R14 with dummy values
        for (uint8_t i = 3; i <= 16; i++)
            threadStacks[NumberOfThreads][STACKSIZE - i] = 1;

        //Sets stack pointer to point to top of stack pointer address
        threadControlBlocks[NumberOfThreads].sp = &threadStacks[NumberOfThreads][STACKSIZE - 16];

        //Increments number of threads
        NumberOfThreads++;

        return SUCCESS;
    }
    else
    {
        return ERROR;
    }
}


/*
 * Adds periodic threads to G8RTOS Scheduler
 * Function will initialize a periodic event struct to represent event.
 * The struct will be added to a linked list of periodic events
 * Param Pthread To Add: void-void function for P thread handler
 * Param period: period of P thread to add
 * Returns: Error code for adding threads
 */
int G8RTOS_AddPeriodicEvent(void (*PthreadToAdd)(void), uint32_t period)
{
    //If number of periodic threads is not equal to maximum, then
    if(NumberOfPthreads != MAXPTHREADS)
    {
        //If number of periodic threads
        if(NumberOfPthreads == 0)
        {
            //Adds a new periodic thread to point to itself
            Pthread[NumberOfPthreads].Handler = PthreadToAdd;
            Pthread[NumberOfPthreads].period = period;
            Pthread[NumberOfPthreads].currentTime = NumberOfPthreads;
            Pthread[NumberOfPthreads].executeTime = period + NumberOfPthreads;
            Pthread[NumberOfPthreads].next = &Pthread[NumberOfPthreads];
            Pthread[NumberOfPthreads].prev = &Pthread[NumberOfPthreads];
        }
        else
        {
            //Adds a new periodic thread to point to itself
            Pthread[NumberOfPthreads].Handler = PthreadToAdd;
            Pthread[NumberOfPthreads].period = period;
            Pthread[NumberOfPthreads].currentTime = NumberOfPthreads;
            Pthread[NumberOfPthreads].executeTime = period + NumberOfPthreads;

            //Makes new periodic thread point to previous and head
            Pthread[NumberOfPthreads].prev = &Pthread[NumberOfPthreads - 1];
            Pthread[NumberOfPthreads].next = &Pthread[0];

            //Makes previous root and head point to new periodic thread
            Pthread[NumberOfPthreads - 1].next = &Pthread[NumberOfPthreads];
            Pthread[0].prev = &Pthread[NumberOfPthreads];
        }
        NumberOfPthreads++;
        return SUCCESS;
    }
    //Returns 0 if more threads cannot be added
    return ERROR;
}

/* TODO
    Alternate Sleeping Implementation
    • Another way to implement sleeping is to remove the new sleeping thread from the
    linked list of active threads, and adding it to new doubly linked list of sleeping threads
    • This new list of sleeping threads will be sorted from smallest to highest sleep count
    • Once the thread with the lowest sleep count equals the system time, that thread is
    woken up
    • Advantage: Now we only have to check one sleeping thread’s sleep count within the
    SysTick handler as opposed to every initialized thread
*/

/*
 * Puts the current thread into a sleep state.
 *  param durationMS: Duration of sleep time in ms
 */
void G8RTOS_Sleep(uint32_t durationMS)
{
    //Initializes currently running threads sleep count
    CurrentlyRunningThread->sleepCount = SystemTime + durationMS;

    //Puts thread to sleep
    CurrentlyRunningThread->asleep = true;

    //Sets PendSV flag, to yield CPU
    SCB->ICSR |= (1<<28);
}

/*********************************************** Public Functions *********************************************************************/
