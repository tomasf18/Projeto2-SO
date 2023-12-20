/**
 *  \file semSharedMemGroup.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the groups:
 *     \li goToRestaurant
 *     \li checkInAtReception
 *     \li orderFood
 *     \li waitFood
 *     \li eat
 *     \li checkOutAtReception
 *
 *  \author Nuno Lau - December 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>

#include "probConst.h"
#include "probDataStruct.h"
#include "logging.h"
#include "sharedDataSync.h"
#include "semaphore.h"
#include "sharedMemory.h"

/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

static void goToRestaurant (int id);
static void checkInAtReception (int id);
static void orderFood (int id);
static void waitFood (int id);
static void eat (int id);
static void checkOutAtReception (int id);


/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the group.
 */
int main (int argc, char *argv[])
{
    int key;                                         /*access key to shared memory and semaphore set */
    char *tinp;                                                    /* numerical parameters test flag */
    int n;   /* Group ID */

    /* validation of command line parameters */
    if (argc != 5) { 
        freopen ("error_GR", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else {
     //  freopen (argv[4], "w", stderr);
       setbuf(stderr,NULL);
    }

    /* Obtenção do Group ID */
    n = (unsigned int) strtol (argv[1], &tinp, 0);
    if ((*tinp != '\0') || (n >= MAXGROUPS )) { 
        fprintf (stderr, "Group process identification is wrong!\n");
        return EXIT_FAILURE;
    }
    strcpy (nFic, argv[2]);
    key = (unsigned int) strtol (argv[3], &tinp, 0);
    if (*tinp != '\0') { 
        fprintf (stderr, "Error on the access key communication!\n");
        return EXIT_FAILURE;
    }

    /* connection to the semaphore set and the shared memory region and mapping the shared region onto the
       process address space */
    if ((semgid = semConnect (key)) == -1) { 
        perror ("error on connecting to the semaphore set");
        return EXIT_FAILURE;
    }
    if ((shmid = shmemConnect (key)) == -1) { 
        perror ("error on connecting to the shared memory region");
        return EXIT_FAILURE;
    }
    if (shmemAttach (shmid, (void **) &sh) == -1) { 
        perror ("error on mapping the shared region on the process address space");
        return EXIT_FAILURE;
    }

    /* initialize random generator */
    srandom ((unsigned int) getpid ());                                                 


    /* simulation of the life cycle of the group -> Indica o que cada Grupo vai fazer */
    goToRestaurant(n);          // Ir para o restaurante
    checkInAtReception(n);      // Fazer chek-in na receção
    orderFood(n);               // Pedir comida
    waitFood(n);                // Esperar pela comida
    eat(n);                     // Comer
    checkOutAtReception(n);     // Fazer check-out na receção

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief normal distribution generator with zero mean and stddev deviation. 
 *
 *  Generates random number according to normal distribution.
 * 
 *  \param stddev controls standard deviation of distribution
 */
static double normalRand(double stddev)
{
   int i;

   double r=0.0;
   for (i=0;i<12;i++) {
       r += random()/(RAND_MAX+1.0);
   }
   r -= 6.0;

   return r*stddev;
}

/**
 *  \brief group goes to restaurant 
 *
 *  The group takes its time to get to restaurant.
 *
 *  \param id group id
 */
static void goToRestaurant (int id)
{
    double startTime = sh->fSt.startTime[id] + normalRand(STARTDEV);

    if (startTime > 0.0) {
        /* O Grupo começa a ir para o restaurante */
        usleep((unsigned int) startTime );
        /* O Grupo chega ao restaurante */
    }
}

/**
 *  \brief group eats
 *
 *  The group takes his time to eat a pleasant dinner.
 *
 *  \param id group id
 */
static void eat (int id)
{
    double eatTime = sh->fSt.eatTime[id] + normalRand(EATDEV);
    
    if (eatTime > 0.0) {
        /* O Grupo começa a comer */
        usleep((unsigned int) eatTime );
        /* O Grupo termina de comer */
    }
}

/**
 *  \brief group checks in at reception
 *
 *  Group should, as soon as receptionist is available, ask for a table,
 *  signaling receptionist of the request.  
 *  Group may have to wait for a table in this method.
 *  The internal state should be saved.
 *
 *  \param id group id
 *
 *  \return true if first group, false otherwise
 */
static void checkInAtReception(int id)
{
    /* O Grupo verifica primeiro se o Receptionist está disponível para falar com eles */
    if (semDown(semgid, sh->receptionistRequestPossible) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* O Receptionist fica aqui disponível para atender o Grupo */



    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* O Grupo atualiza o seu estado para "na receção (à espera)" */
    sh->fSt.st.groupStat[id] = ATRECEPTION;
    saveState(nFic, &sh->fSt);

    /* O Grupo faz o pedido ao Receptionist */
    sh->fSt.receptionistRequest.reqGroup = id;
    sh->fSt.receptionistRequest.reqType = TABLEREQ;

    if (semUp (semgid, sh->mutex) == -1) {                                                      /* exit critical region */
        perror ("error on the up operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    

    /* O Grupo avisa (acorda) o Receptionist que um novo pedido está disponível para ele na memória partilhada" */
    if (semUp(semgid, sh->receptionistReq) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* Agora, os grupos precisam de esperar que uma mesa fique disponível */ 
    if (semDown(semgid, sh->waitForTable[id]) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

}

/**
 *  \brief group orders food.
 *
 *  The group should update its state, request food to the waiter and 
 *  wait for the waiter to receive the request.
 *  
 *  The internal state should be saved.
 *
 *  \param id group id
 */
static void orderFood (int id)
{
    /* Primeiro, o Grupo precisa de verificar se o Waiter está disponível para falar com eles */
    if (semDown(semgid, sh->waiterRequestPossible) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* A partir daqui, o Waiter está disponível para atender o Grupo */



    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* O Grupo precisa de atualizar o seu estado para "a pedir a comida" */
    sh->fSt.st.groupStat[id] = FOOD_REQUEST;
    saveState(nFic, &sh->fSt);

    /* E precisa de colocar o pedido ao Waiter (na zona partilhada) */
    sh->fSt.waiterRequest.reqGroup = id;
    sh->fSt.waiterRequest.reqType = FOODREQ;

    /* Esta variável serve para saber qual é a mesa em que o Grupo se encontra, para depois ser usada no Waiter acknowledge de que anotou o pedido */
    int assignedTable = sh->fSt.assignedTable[id];

    if (semUp (semgid, sh->mutex) == -1) {                                                     /* exit critical region */
        perror ("error on the up operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }



    /* Depois do pedido de comida se encontrar na zona partilhada, o Grupo diz ao Waiter que pode lá ir lê-lo */
    if (semUp(semgid, sh->waiterRequest) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* Depois do pedido feito, o Grupo precisa de esperar para saber se o Waiter anotou tudo bem */
    if (semDown(semgid, sh->requestReceived[assignedTable]) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
}

/**
 *  \brief group waits for food.
 *
 *  The group updates its state, and waits until food arrives. 
 *  It should also update state after food arrives.
 *  The internal state should be saved twice.
 *
 *  \param id group id
 */
static void waitFood (int id)
{
    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* O Grupo atualiza o seu estado para "à espera da comida" */
    sh->fSt.st.groupStat[id] = WAIT_FOR_FOOD;
    saveState(nFic, &sh->fSt);

    /* Esta variável serve para saber qual é a mesa em que o Grupo se encontra, para depois ser usada no Waiter acknowledge de que a comida chegou */
    int assignedTable = sh->fSt.assignedTable[id];

    if (semUp (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }



    /* Aqui, o grupo precisa de esperar para que a comida chegue (ou seja, que o Waiter dê semUp ao seguinte semáformo) */
    if (semDown(semgid, sh->foodArrived[assignedTable]) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* Depois de conseguir dar semDown, quer dizer que a comida chegou à mesa, e que o Grupo pode começar a comer */
    


    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* Sendo que pode começar a comer, então atualiza o seu estado para "a comer" */
    sh->fSt.st.groupStat[id] = EAT;
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
}

/**
 *  \brief group check out at reception. 
 *
 *  The group, as soon as receptionist is available, updates its state and 
 *  sends a payment request to the receptionist.
 *  Group waits for receptionist to acknowledge payment. 
 *  Group should update its state to LEAVING, after acknowledge.
 *  The internal state should be saved twice.
 *
 *  \param id group id
 */
static void checkOutAtReception (int id)
{
    /* O Grupo verifica se o Receptionist está disponível para falar com eles */
    if (semDown(semgid, sh->receptionistRequestPossible) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* O Receptionist fica aqui disponível para atender o Grupo */



    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* Sendo que o Receptionist já se encontra disponível, o Grupo atualiza o seu estado para "a pagar/checkout" */
    sh->fSt.st.groupStat[id] = CHECKOUT;
    saveState(nFic, &sh->fSt);

    /* O Grupo faz o pedido de pagamento ao Receptionist */
    sh->fSt.receptionistRequest.reqGroup = id;
    sh->fSt.receptionistRequest.reqType = BILLREQ;

    /* Esta variável serve para saber qual é a mesa em que o Grupo se encontra, para depois ser usada no Receptionist acknowledge de que o pagamento está feito */
    int assignedTable = sh->fSt.assignedTable[id];

    if (semUp (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }



    /* Depois do pedido de pagamento se encontrar na zona partilhada, o Grupo diz ao Receptionist que pode lá ir lê-lo */
    if (semUp(semgid, sh->receptionistReq) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* O Grupo agora precisa de esperar que o Receptionist diga que o pagamento está feito e que podem ir embora */
    if (semDown(semgid, sh->tableDone[assignedTable]) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* Depois de dar semDown, significa que o pagamento está feito e que pode ir embora */



    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* Agora que pagaram, o Grupo atualiza o seu estado para "a ir embora" */
    sh->fSt.st.groupStat[id] = LEAVING;
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

}

