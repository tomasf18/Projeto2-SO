/**
 *  \file semSharedWaiter.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the waiter:
 *     \li waitForClientOrChef
 *     \li informChef
 *     \li takeFoodToTable
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
#include <assert.h>

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

/** \brief waiter waits for next request */
static request waitForClientOrChef ();

/** \brief waiter takes food order to chef */
static void informChef(int group);

/** \brief waiter takes food to table */
static void takeFoodToTable (int group);




/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the waiter.
 */
int main (int argc, char *argv[])
{
    int key;                                            /*access key to shared memory and semaphore set */
    char *tinp;                                                       /* numerical parameters test flag */

    /* validation of command line parameters */
    if (argc != 4) { 
        freopen ("error_WT", "a", stderr);
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

    /* simulation of the life cycle of the waiter -> Indica o que o Waiter vai fazer */
    int nReq = 0;
    request req;
    /* Enquanto o nº de requests for inferior ao máximo de requests possível para o Waiter, executa este loop */
    while( nReq < sh->fSt.nGroups*2 ) {     // nGroups (5) * 2 (requests feitos por Groups e requests feitos por Chef; a cada pedido do Grupo, existe uma resposta do Chef) = nTotalRequests
        req = waitForClientOrChef();        // Waiter anota o pedido do Grupo/Chef
        switch(req.reqType) {
            /* Se for o Grupo a fazer um pedido de refeição, então vai informar o Chef */
            case FOODREQ:  
                   informChef(req.reqGroup); // Além de informar o pedido de comida, também diz qual foi o grupo que o fez
                   break;
            /* Se o pedido for do Chef para levar a comida pronta à mesa, então o Waiter leva à mesa respetiva */       
            case FOODREADY: 
                   takeFoodToTable(req.reqGroup); // Leva a comida para a mesa onde está o grupo indicado como argumento
                   break;
        }
        /* Incrementa o nº de pedidos */
        nReq++; 
    }

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief waiter waits for next request 
 *
 *  Waiter updates state and waits for request from group or from chef, then reads request.
 *  The waiter should signal that new requests are possible.
 *  The internal state should be saved.
 *
 *  \return request submitted by group or chef
 */
static request waitForClientOrChef()
{
    request req; 

    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* 
        O Waiter atualiza o seu estado para "à espera de um pedido" (de um Grupo para fazer 
        um pedido, ou do Chef para levar a comida à mesa) 
    */
    sh->fSt.st.waiterStat = WAIT_FOR_REQUEST;

    /* Salva-se o estado interno */
    saveState(nFic, &sh->fSt);
    
    if (semUp (semgid, sh->mutex) == -1)      {               /* exit critical region */
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //
   
    /*
        O Waiter precisa de ficar à espera de que um pedido seja feito, para depois poder 
        aceder-lhe na região crítica. Isto é, o pedido "request" (da estrutura "request" da 
        memória partilhada) vai ser atualizado na memória partilhada por Group ou por Chef, 
        e depois, quando fizerem Up do semáforo abaixo, dizendo que já lá colocaram o pedido 
        e que a região crítica está livre, o Waiter poderá lê-lo
    */
    if (semDown(semgid, sh->waiterRequest) == -1)      {                                             
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* 
        Nesta linha, quando Waiter consegue fazer semDown, significa que o pedido já foi 
        colocado na região partilhada da memória 
    */


    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* Então, o Waiter lê o pedido que foi colocado por Group ou Chef na região crítica */
    req = sh->fSt.waiterRequest;

    if (semUp(semgid, sh->mutex) == -1) {                     /* exit critical region */
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //

    /* 
        Agora, depois de ler o pedido acima, e de estar já a exeutar o mesmo, o Waiter 
        avisa que está disponível para novos pedidos (pois, segundo a main(), a entidade 
        executa tudo de forma sequencial: 
            Recebe o Pedido do Chef/Grupo -> Leva o Pedido ao Grupo/Chef -> Recebe Outro Pedido
        Ou seja, assim que anota o pedido de um, podemos já fazê-lo dar Up do semáforo para outro 
        poder pedir, sendo que o anterior já está a ser executado.) 
    */
    if (semUp(semgid, sh->waiterRequestPossible) == -1)      {                                             
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    return req;
}

/**
 *  \brief waiter takes food order to chef 
 *
 *  Waiter updates state and then takes food request to chef.
 *  Waiter should inform group that request is received.
 *  Waiter should wait for chef receiving request.
 *  The internal state should be saved.
 *
 */
static void informChef (int n)
{
    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* O Waiter atualiza o seu estado para "a ir informar Chef sobre o pedido do Grupo 'n'" */
    sh->fSt.st.waiterStat = INFORM_CHEF;

    /* 
        O Waiter indica ao Chef que existe um pedido pendente e indica o grupo que o fez, 
        acedendo à memória partilhada para o fazer 
    */
    sh->fSt.foodOrder = 1;
    sh->fSt.foodGroup = n;

    /* 
        Aqui, o Waiter obtém a mesa que foi dada ao grupo n, para depois poder dar o acknowledge 
        respetivo ao Grupo, sobre ter anotado o pedido 
    */
    int assignedTable = sh->fSt.assignedTable[n];
    
    /* Salva-se o estado interno */
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1)                      /* exit critical region */
    { perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //

    /* O Waiter avisa o Grupo n de que anotou corretamente o seu pedido */
    if (semUp (semgid, sh->requestReceived[assignedTable]) == -1)      {                                             
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    } 

    /* 
        O Waiter leva o pedido ao Chef, dizendo-lhe que pode parar de esperar pelo pedido 
        e começar a cozinhar (COOK) 
    */
    if (semUp (semgid, sh->waitOrder) == -1)      {                                             
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* O Waiter espera agora que o Chef lhe confirma que recebeu o pedido que lhe passou */
    if (semDown (semgid, sh->orderReceived) == -1)      {                                             
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    } 
}

/**
 *  \brief waiter takes food to table 
 *
 *  Waiter updates its state and takes food to table, allowing the meal to start.
 *  Group must be informed that food is available.
 *  The internal state should be saved.
 *
 */

static void takeFoodToTable (int n)
{
    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* O Waiter atualiza o seu estado para "a ir levar a comida à mesa do grupo 'n'" */
    sh->fSt.st.waiterStat = TAKE_TO_TABLE;

    /* 
        Aqui, o Waiter obtém a mesa que foi dada ao grupo n, para depois poder dar o 
        acknowledge respetivo ao Grupo, sobre a comida ter chegado 
    */
    int assignedTable = sh->fSt.assignedTable[n];

    /* Salva-se o estado interno */
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1)  {                   /* exit critical region */
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //

    /* 
        O Waiter informa o grupo 'n' que a comida está pronta e já chegou, e que podem, 
        então começar a comer (EAT) 
    */
    if (semUp(semgid, sh->foodArrived[assignedTable]) == -1)      {                                             
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }     
}

