/**
 *  \file semSharedMemChef.c (implementation file) 
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the chef:
 *     \li waitOrder
 *     \li processOrder
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
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

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

/** \brief group that requested cooking food */
static int lastGroup;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

static void waitForOrder ();
static void processOrder ();

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the chef.
 */
int main (int argc, char *argv[])
{
    int key;                                          /*access key to shared memory and semaphore set */
    char *tinp;                                                     /* numerical parameters test flag */

    /* validation of command line parameters */

    if (argc != 4) { 
        freopen ("error_CH", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else {
       freopen (argv[3], "w", stderr);
       setbuf(stderr,NULL);
    }
    strcpy (nFic, argv[1]);
    key = (unsigned int) strtol (argv[2], &tinp, 0);
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

    /* simulation of the life cycle of the chef -> Indica o que o Chef vai fazer */

    int nOrders = 0;
    /* Enquanto o nº de pedidos de comida (Group Orders) for inferior ao máximo de orders possível para o Chef, executa este loop */
    while (nOrders < sh->fSt.nGroups) {     // O Chef apenas responde aos nGroups, com as nOrders que eles fazem ao Waiter, e que este lhe reencaminha
       /* Espera por uma order de um grupo, para poder cozinhar */
       waitForOrder();
       /* Processa a order recebida acima, isto é, cozinha e entrega o pedido ao Waiter para este levar à mesa respetiva */
       processOrder();
       /* Incrementa o nº de pedidos */
       nOrders++;
    }

    /* unmapping the shared region off the process address space */

    if (shmemDettach (sh) == -1) { 
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief chefs wait for a food order.
 *
 *  The chef waits for the food request that will be provided by the waiter. 
 *  Updates its state and saves internal state. 
 *  Received order should be acknowledged. 
 */
static void waitForOrder ()
{
    /* O Chef começa sempre por aguardar um pedido (vindo do Waiter, que reencaminha do Grupo) */
    if (semDown(semgid, sh->waitOrder) == -1) {                                                    
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* 
        O Chef recebe o pedido assim que puder dar semDown do semáforo anterior (isto é, assim que Waiter 
        dá semUp a informar que existe um novo pedido), logo nesta linha já o recebeu* 
    */

    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1) {                 /* enter critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* 
        Aqui, a partir da informação que recebe do Waiter, o Chef atualiza o lastGroup para o grupo que vem no pedido. 
        Esta variável será sempre precisa para que depois o Waiter consiga levar o pedido à mesa certa.
    */
    lastGroup = sh->fSt.foodGroup; // 'sh->fSt.foodGroup' tem o grupo associado ao pedido que o Chef recebeu
    /* Sendo que já recebeu um novo pedido, então atualiza o seu estado para "a cozinhar" */
    sh->fSt.st.chefStat = COOK;
    
    /* Salva-se o estado interno */
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                    /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //

    /* 
        Aqui, o Chef notifica o Waiter de que recebeu corretamente o pedido, 
        para que o Waiter possa ir atender outros pedidos 
    */
    if (semUp(semgid, sh->orderReceived) == -1) {                                                    
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }     
}

/**
 *  \brief chef cooks, then delivers the food to the waiter 
 *
 *  The chef takes some time to cook and signals the waiter that food is 
 *  ready (this may only happen when waiter is available) (waiterRequestPossible)
 *  then updates its state. (to WAIT_FOR_ORDER)
 *  The internal state should be saved.
 */
static void processOrder ()
{
    /* 
        O Chef começa a cozinhar no final da função waitForOrder() definida 
        acima, quando passa para o estado COOK, demorando o tempo seguinte a fazê-lo: 
    */
    usleep((unsigned int) floor ((MAXCOOK * random ()) / RAND_MAX + 100.0));
    /* *O Chef termina de cozinhar* */

    /* O Chef espera pelo Waiter para que este fique disponível para levar a comida */
    if (semDown(semgid, sh->waiterRequestPossible) == -1) {                                                     
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* 
        O Waiter fica disponível assim que Chef puder dar semDown ao semáforo acima (isto porque o 
        Waiter dá semUp quando se encontra disponível para ouvir novos pedidos), logo, nesta linha, 
        o Chef já pode pedir ao Waiter para levar a comida à mesa*
    */

   // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1) {                                  /* enter critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* 
        Sendo que, acima, a variável 'lastGroup' foi usada para armazenar o ID do grupo que fez o pedido, 
        aqui vai-se certificar de que o pedido vai ser entregue pelo Waiter ao grupo certo, que tem esse ID 
        (função main()->takeFoodToTable() do Waiter), passando para a memória partilhada 
        ('sh->fSt.waiterRequest.reqGroup' e 'sh->fSt.waiterRequest.reqType') tanto o ID desse grupo, 
        como o tipo de request que o Waiter irá receber do Chef
    */ 
    sh->fSt.waiterRequest.reqGroup = lastGroup; 
    sh->fSt.waiterRequest.reqType = FOODREADY;

    /* Indica que terminou um pedido, passando a variável/flag em memória partilhada de novo para 0 */
    sh->fSt.foodOrder = 0;

    /* Atualiza o seu estado, de novo para "à espera de um novo pedido" */
    sh->fSt.st.chefStat = WAIT_FOR_ORDER;

    /* Salvar as alterações efetuadas em memória partilhada (para imprimi-las corretamente em logging.c) */
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1) {                                      /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //
    
    /* Por fim, o Chef informa o Waiter de que pode obter o pedido pronto e levá-lo */
    if (semUp(semgid, sh->waiterRequest) == -1) {                                                      
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }
}

