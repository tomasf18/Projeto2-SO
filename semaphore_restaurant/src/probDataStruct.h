/**
 *  \file probDataStruct.h (interface file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Definition of internal data structures.
 *
 *  They specify internal metadata about the status of the intervening entities.
 *
 *  \author Nuno Lau - December 2023
 */

#ifndef PROBDATASTRUCT_H_
#define PROBDATASTRUCT_H_

#include <stdbool.h>

#include "probConst.h"

/**
 *  \brief Definition of requests to receptionist and waiter 
 */
typedef struct {
    /** \brief request id */
    int reqType;
    /** \brief group that issues the request (dummy/fictício if request source is chef) */
    int reqGroup;
} request;


/**
 *  \brief Definition of <em>state of the intervening entities</em> data type. -> Estado de todas as entidades que participam
 */
typedef struct {
    /** \brief receptionist state */
    unsigned int receptionistStat;
    /** \brief waiter state */
    unsigned int waiterStat;
    /** \brief chef state */
    unsigned int chefStat;
    /** \brief group state array */
    unsigned int groupStat[MAXGROUPS];

} STAT;


/**
 *  \brief Definition of <em>full state of the problem</em> data type. 
 */
typedef struct
{   /** \brief state of all intervening entities */
    STAT st; // dentro deste temos os estados todos

    /** \brief number of groups */
    int nGroups;
    /** \brief number of groups waiting for table */
    int groupsWaiting;

    /** \brief estimated start time of groups */
    int startTime[MAXGROUPS];
    /** \brief estimated eat time of groups */
    int eatTime[MAXGROUPS];

    /** \brief saves the table that is being used by each group */
    int assignedTable[MAXGROUPS];

    /** \brief flag of food request from waiter to chef */
    int foodOrder;
    /** \brief group associated to food request from waiter to chef */
    int foodGroup;


    /** \brief used by groups to store request to receptionist */
    request receptionistRequest;

    /** \brief used by groups and chef to store request to waiter */
    request waiterRequest; // Antes de acordar o waiter, colocam aqui o pedido para o waiter


} FULL_STAT;


#endif /* PROBDATASTRUCT_H_ */
