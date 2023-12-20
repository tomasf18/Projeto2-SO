/**
 *  \file sharedDataSync.h (interface file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  \brief Definition of the shared data and the synchronization devices.
 *
 *  Both the format of the shared data, which represents the full state of the problem, and the identification of
 *  the different semaphores, which carry out the synchronization among the intervening entities, are provided.
 *
 *  \author Nuno Lau - December 2023
 */

#ifndef SHAREDDATASYNC_H_
#define SHAREDDATASYNC_H_

#include "probConst.h"
#include "probDataStruct.h"

// UMA DAS PRIMEIRAS COISAS QUE DEVEMOS FAZER É UMA TABELA EM QUE SE METEM OS SEMAFOROS E A FORMA COMO O VAMOS USAR, OU SEJA:
//    SEMÁFORO      QUEM ESPERA       QUANDO? FUNÇÃO?     QUEM FAZ UP?      QUANDO? FUNÇÃO?
//      ...           ...               ....                  ...               ...

// QUEM ESPERA É O CHEFE, POR ISSO ELE DÁ DOWN

/**
 *  \brief Definition of <em>shared information</em> data type.
 */
typedef struct
        { /** \brief full state of the problem */
          FULL_STAT fSt;
          // Vamos ter uma regiao critica quando quando qualquer processo quer aceder à memoria partilhada
          /* semaphores ids */
          /** \brief identification of critical region protection semaphore – val = 1 */
          unsigned int mutex; // usado para ninguem entrar na shared memory
          /** \brief identification of semaphore used by receptionist to wait for groups - val = 0 */
          unsigned int receptionistReq; // Por exemplo, aqui, quem vai fazer down neste semáforo é o receptionist e quem faz up é o group "receptionist wait for groups"
          /** \brief identification of semaphore used by groups to wait before issuing receptionist request - val = 1 */
          unsigned int receptionistRequestPossible;
          /** \brief identification of semaphore used by waiter to wait for requests – val = 0  */
          unsigned int waiterRequest; // waiter faz down para esperar pelos requests
                                      // Antes de voltar a fazer up (acordar), o request já terá de ter sido guardado numa variável waiterReq
                                      // Implementar o semaforo da maneira mais intuitiva pra nós: o semaforo nunca tem valores negativos, n mínimo é zero, caso esteja a 0 e faça down, bloqueia
          /** \brief identification of semaphore used by groups and chef to wait before issuing waiter request - val = 1 */
          unsigned int waiterRequestPossible;
          /** \brief identification of semaphore used by chef to wait for order – val = 0  */
          unsigned int waitOrder;
          /** \brief identification of semaphore used by waiter to wait for chef – val = 0  */
          unsigned int orderReceived;
          /** \brief identification of semaphore used by groups to wait for table – val = 0 */
          unsigned int waitForTable[MAXGROUPS]; // arrray de semaforos com o tamanho de maxgroups (para cada grupo poder ter o seu semáforo waitForTable)
          /** \brief identification of semaphore used by groups to wait for waiter ackowledge – val = 0  */
          unsigned int requestReceived[NUMTABLES]; //!!!!!!!!!!!!!!!!!
          /** \brief identification of semaphore used by groups to wait for food – val = 0 */
          unsigned int foodArrived[NUMTABLES];
          /** \brief identification of semaphore used by groups to wait for payment completed – val = 0 */
          unsigned int tableDone[NUMTABLES];

        } SHARED_DATA; // -> O que vai estar em memoria partilhada

// o grupo faz o pedido da comida, mas depois de o fazer, não pode começar a comer

/** \brief number of semaphores in the set */
#define SEM_NU               ( 7 + sh->fSt.nGroups + 3*NUMTABLES )

// Temos 7 semafofos individuais, até Orderreceived
// o 1o semaforo de foodarraived é waitfortabel+numero de grupos
// cada grupo vai esperar no semaforo waitfortable pela mesa
// quem vai fazer up é o rececionista, que atribui a mesa

#define MUTEX                        1
#define RECEPTIONISTREQ              2
#define RECEPTIONISTREQUESTPOSSIBLE  3
#define WAITERREQUEST                4
#define WAITERREQUESTPOSSIBLE        5
#define WAITORDER                    6
#define ORDERRECEIVED                7
#define WAITFORTABLE                 8 // a partir daqui tratam se de arrays de semáforos
#define FOODARRIVED                  (WAITFORTABLE+sh->fSt.nGroups)
// Para indicar um handshaking, ou seja, quando o grupo coloca o pedido na memoria partilhada, depois acorda o 
//rececionista, mas não avança enquanto não receber o aviso do rececionista que recebeu o pedido
#define REQUESTRECEIVED              (FOODARRIVED+NUMTABLES)
#define TABLEDONE                    (REQUESTRECEIVED+NUMTABLES)

#endif /* SHAREDDATASYNC_H_ */
