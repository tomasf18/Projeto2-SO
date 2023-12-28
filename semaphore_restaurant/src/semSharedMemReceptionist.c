/**
 *  \file semSharedReceptionist.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the receptionist:
 *     \li waitForGroup
 *     \li provideTableOrWaitingRoom
 *     \li receivePayment
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

/* constants for groupRecord */
#define TOARRIVE 0
#define WAIT     1
#define ATTABLE  2
#define DONE     3

/** \brief receptioninst view on each group evolution (useful to decide table binding) */
static int groupRecord[MAXGROUPS];


/** \brief receptionist waits for next request */
static request waitForGroup ();

/** \brief receptionist waits for next request */
static void provideTableOrWaitingRoom (int n);

/** \brief receptionist receives payment */
static void receivePayment (int n);



/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the receptionist.
 */
int main (int argc, char *argv[])
{
    int key;                                            /*access key to shared memory and semaphore set */
    char *tinp;                                                       /* numerical parameters test flag */

    /* validation of command line parameters */
    if (argc != 4) { 
        freopen ("error_RT", "a", stderr);
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


    /* initialize internal receptionist memory -> Coloca todos os grupos como "a chegar" */
    int g;
    for (g = 0; g < sh->fSt.nGroups; g++) {
       groupRecord[g] = TOARRIVE;
    }

    /* simulation of the life cycle of the receptionist -> Indica o que o Receptionist vai fazer */
    int nReq = 0;
    request req;
    /* Enquanto o nº de requests for inferior ao máximo de requests possível para o Receptionist, executa este loop */
    while( nReq < sh->fSt.nGroups*2 ) {     // nGroups (5) * 2 (requests feitos por Groups, um para mesa, outro para pagamento) = nTotalRequests
        req = waitForGroup();               // Receptionist ouve o pedido do Grupo
        switch(req.reqType) {
            /* Se for um pedido de mesa, então atribui-lhes uma mesa, assim que possível */
            case TABLEREQ:
                   provideTableOrWaitingRoom(req.reqGroup); //TODO param should be groupid
                   break;
            case BILLREQ:
            /* Se for um pedido para pagamento, então recebe-o */
                   receivePayment(req.reqGroup);
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
 *  \brief decides table to occupy for group n or if it must wait.
 *
 *  Checks current state of tables and groups in order to decide table or wait.
 *
 *  \return table id or -1 (in case of wait decision)
 */
static int decideTableOrWait(int n) // Este método é chamado entre semáforos mutex, logo pode aceder à região partilhada sem problemas
{   
    /* 
        Sabe-se que, se um grupo não está a ocupar nenhuma mesa, o valor do índice que corresponde ao seu id no array 
        'sh->fSt.assignedTable' é -1 (ver main() em 'probSemSharedMemRestaurant.c') 
    */
    int mesa0 = 1;                                  // Mesa 0 inicialmente disponível
    int mesa1 = 1;                                  // Mesa 1 inicialmente disponível
    /* Percorreu-se o array tal como é feito na main() acima */
    for (int group = 0; group < sh->fSt.nGroups; group++) { 
        if (sh->fSt.assignedTable[group] == 0) {
            mesa0 = 0; // Se a condição for verdadeira, significa que a mesa 0 não está disponível
        }
        if (sh->fSt.assignedTable[group] == 1) {
            mesa1 = 0; // Se a condição for verdadeira, significa que a mesa 1 não está disponível
        }
    }    

    /* Agora, tem de se verificar qual é que pode ser atribuída (se ambas tiverem sido assinaladas como ocupadas, retorna-se -1) */
    if (mesa0)
        return 0;
    else if(mesa1)
        return 1; 
    else
        return -1;       
}

/**
 *  \brief called when a table gets vacant and there are waiting groups 
 *         to decide which group (if any) should occupy it.
 *
 *  Checks current state of tables and groups in order to decide group.
 *
 *  \return group id or -1 (in case of wait decision) -> Não seria "in case of no group waiting"
 */
static int decideNextGroup() // Este método é chamado entre semáforos mutex, logo pode aceder à região partilhada sem problemas
{
    /* O ciclo seguinte encontra um grupo em espera e retorna o seu id, se existir algum */
    for (int group = 0; group < sh->fSt.nGroups; group++) {
        if (groupRecord[group] == WAIT) {
            return group;
        }
    }

    return -1;
}

/**
 *  \brief receptionist waits for next request 
 *
 *  Receptionist updates state and waits for request from group, then reads request,
 *  and signals availability for new request.
 *  The internal state should be saved.
 *
 *  \return request submitted by group
 */
static request waitForGroup()
{
    request ret; 

    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    
    /* O Receptionist atualiza e salva o seu estado para "à espera de pedido de um Grupo" */
    sh->fSt.st.receptionistStat = WAIT_FOR_REQUEST;
    saveState(nFic, &sh->fSt);
    
    if (semUp (semgid, sh->mutex) == -1) {                    /* exit critical region */
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //

    /* 
        Depois de atualizar o estado, o Receptionist tem de ficar à espera que o Grupo 
        lhe peça alguma coisa 
    */
    if (semDown(semgid, sh->receptionistReq) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }
    /* 
        Assim que conseguir fazer semDown, significa que pode aceder à zona crítica 
        para ler o pedido que o Grupo lá colocou 
    */

    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* Agora que o Grupo colocou o pedido na memória partilhada, o Receptionist pode lê-lo */
    ret = sh->fSt.receptionistRequest;

    if (semUp (semgid, sh->mutex) == -1) {                    /* exit critical region */
     perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //
    
    /* 
        O Rececionista avisa que, depois de atender ao pedido lido acima, fica disponível logo 
        a seguir para outro (visto que, tal como em Waiter, as tarefas são executadas de forma sequencial) 
    */
    if (semUp(semgid, sh->receptionistRequestPossible) == -1) {                                                
        perror ("error on the down operation for semaphore access (CT)");
        exit (EXIT_FAILURE);
    }

    /* Devolve-se o pedido para o usar na main() */
    return ret;
}
         

/**
 *  \brief receptionist decides if group should occupy table or wait
 *
 *  Receptionist updates state and then decides if group occupies table
 *  or waits. Shared (and internal) memory may need to be updated.
 *  If group occupies table, it must be informed that it may proceed. 
 *  The internal state should be saved.
 *
 */
static void provideTableOrWaitingRoom (int n)
{
    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* O Receptionist atualiza o seu estado para "a atribuir mesa ao grupo n" */
    sh->fSt.st.receptionistStat = ASSIGNTABLE;

    /* Verifica-se se existe alguma mesa disponível (ver função 'decideTableOrWait()', explicada acima) */
    int mesa = decideTableOrWait(n);
    if (mesa == -1) {
        /* 
            Se ambas as mesas estiverem ocupadas, o Receptionist atualiza o seu groupRecord do Grupo 'n' 
            para "esperar", e o nº de grupos à espera aumenta. Como o Grupo já se encontrava à espera de 
            uma mesa (fazendo semDown de waitForTable[]), então aqui não se faz nada relativamente 
            a isso, i.e., ele continua à espera.
        */
        groupRecord[n] = WAIT;
        sh->fSt.groupsWaiting++;
    } else {
        /*  
            Se houver alguma mesa disponível, então este grupo fica com ela e o Receptionist avisa-o de que podem 
            entrar para a mesa (semUp), atualizando, também, o seu groupRecord (não é necessário decrementar 
            'sh->fSt.groupsWaiting')
        */
        sh->fSt.assignedTable[n] = mesa;
        groupRecord[n] = ATTABLE;
        if (semUp(semgid, sh->waitForTable[n]) == -1) {                                            
            perror ("error on the down operation for semaphore access (WT)");
            exit (EXIT_FAILURE);
        }
    }

    /* Salvam-se as alterações feitas */
    saveState(nFic, &sh->fSt);
    
    if (semUp (semgid, sh->mutex) == -1) {                    /* exit critical region */
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ //
}

/**
 *  \brief receptionist receives payment 
 *
 *  Receptionist updates its state and receives payment.
 *  If there are waiting groups, receptionist should check if table that just became
 *  vacant should be occupied. Shared (and internal) memory should be updated.
 *  The internal state should be saved.
 *
 */

static void receivePayment (int n)
{
    // ------------------------------ [Região crítica] ------------------------------ //
    if (semDown (semgid, sh->mutex) == -1)  {                /* enter critical region */
        perror ("error on the up operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* O Receptionist atualiza o seu estado para "a receber o pagamento" */
    sh->fSt.st.receptionistStat = RECVPAY;
    
    /* 
        Esta variável (assignedTable) serve para saber qual é a mesa em que o Grupo se 
        encontra, para depois ser usada no Receptionist acknowledge de que o pagamento 
        está feito, e ainda para ser atribuída a algum grupo que esteja em espera 
    */
    int assignedTable = sh->fSt.assignedTable[n];

    /* O Receptionist atualiza o estado da mesa, dizendo que já se encontra disponível */
    sh->fSt.assignedTable[n] = -1;

    /* Verifica-se se ainda existem grupos à espera */
    int nextGroup = decideNextGroup();

    /* E, se existirem: */
    if (nextGroup != -1) {
        /* Atribui-se ao próximo Grupo a mesa que acabou de ser disponibilizada */
        sh->fSt.assignedTable[nextGroup] = assignedTable;
        /* Avisa-se o grupo que podem ir para a mesa (podem deixar de esperar pela mesa) */
        if (semUp(semgid, sh->waitForTable[nextGroup]) == -1) {                                         
            perror ("error on the down operation for semaphore access (WT)");
            exit (EXIT_FAILURE);
        }
        /* Atualiza-se o groupRecord deste grupo para ATTABLE */
        groupRecord[nextGroup] = ATTABLE;
        /* E decrementa-se a variável que contém o nº de grupos à espera de mesa */
        sh->fSt.groupsWaiting--;
    }

    /* No final, salvam-se e escrevem-se as alterações efetuadas (logging.c) */
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1)  {                   /* exit critical region */
     perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }
    // ------------------------------------------------------------------------------ // 

    /* Aqui, confirma ao Grupo que pagou que o pagamento foi bem sucedido */
    if (semUp(semgid, sh->tableDone[assignedTable]) == -1) {                                         
        perror ("error on the down operation for semaphore access (WT)");
        exit (EXIT_FAILURE);
    }

    /* Finalmente, o Receptionist atualiza o seu groupRecord do grupo 'n' para "feito" */
    groupRecord[n] = DONE;
}

