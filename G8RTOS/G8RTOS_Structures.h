/*
 * G8RTOS_Structure.h
 *
 *  Created on: Jan 12, 2017
 *      Author: Raz Aloni
 */

#ifndef G8RTOS_STRUCTURES_H_
#define G8RTOS_STRUCTURES_H_

#include "G8RTOS.h"
#include <stdbool.h>

/*********************************************** Data Structure Definitions ***********************************************************/

/*
 *  Thread Control Block:
 *      - Every thread has a Thread Control Block
 *      - The Thread Control Block holds information about the Thread Such as the Stack Pointer, Priority Level, and Blocked Status
 *      - For Lab 2 the TCB will only hold the Stack Pointer, next TCB and the previous TCB (for Round Robin Scheduling)
 */

typedef struct tcb_t
{
    /*
    bool alive;
    uint8_t priority;*/
    int32_t* sp; //Holds pointer to stack pointer for respective tcb_t
    struct tcb_t *prev; //Holds pointer to previous tcb_t
    struct tcb_t *next; //HOlds pointer to next tcb_t
    bool asleep; //Tells if thread is asleep
    uint32_t sleepCount; //Holds time wanted to sleep
    semaphore_t *blocked; // 0(not blocked) or semaphore thread  that is currently being waited for.

}tcb_t;


/*
 *  Periodic Thread Control Block:
 *      - Holds a function pointer that points to the periodic thread to be executed
 *      - Has a period in us
 *      - Holds Current time
 *      - Contains pointer to the next periodic event - linked list
 */
typedef struct ptcb_t
{
    void (*Handler)(void); //Function pointer
    uint32_t period; //Holds period
    uint32_t executeTime; //Holds time that will be executed
    uint32_t currentTime; //Default starting value
    struct ptcb_t *prev; //Holds previous periodic thread
    struct ptcb_t *next; //Holds next periodic thread

}ptcb_t;

/*********************************************** Data Structure Definitions ***********************************************************/


/*********************************************** Public Variables *********************************************************************/

tcb_t *CurrentlyRunningThread;

/*********************************************** Public Variables *********************************************************************/




#endif /* G8RTOS_STRUCTURES_H_ */
