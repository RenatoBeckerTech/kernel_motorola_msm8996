/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

//==============================================================================
// Double-link list definitions (adapted from Atheros SDIO stack)
//
// Author(s): ="Atheros"
//==============================================================================

#ifndef __DL_LIST_H___
#define __DL_LIST_H___

#define A_CONTAINING_STRUCT(address, struct_type, field_name)\
            ((struct_type *)((char *)(address) - (char *)(&((struct_type *)0)->field_name)))

/* list functions */
/* pointers for the list */
typedef struct _DL_LIST {
    struct _DL_LIST *pPrev;
    struct _DL_LIST *pNext;
}DL_LIST, *PDL_LIST;
/*
 * DL_LIST_INIT , initialize doubly linked list
*/
#define DL_LIST_INIT(pList)\
    {(pList)->pPrev = pList; (pList)->pNext = pList;}

/* faster macro to init list and add a single item */    
#define DL_LIST_INIT_AND_ADD(pList,pItem) \
{   (pList)->pPrev = (pItem); \
    (pList)->pNext = (pItem); \
    (pItem)->pNext = (pList); \
    (pItem)->pPrev = (pList); \
}
    
#define DL_LIST_IS_EMPTY(pList) (((pList)->pPrev == (pList)) && ((pList)->pNext == (pList)))
#define DL_LIST_GET_ITEM_AT_HEAD(pList) (pList)->pNext
#define DL_LIST_GET_ITEM_AT_TAIL(pList) (pList)->pPrev
/*
 * ITERATE_OVER_LIST pStart is the list, pTemp is a temp list member
 * NOT: do not use this function if the items in the list are deleted inside the
 * iteration loop
*/
#define ITERATE_OVER_LIST(pStart, pTemp) \
    for((pTemp) =(pStart)->pNext; pTemp != (pStart); (pTemp) = (pTemp)->pNext)

static __inline bool DL_ListIsEntryInList(const DL_LIST * pList, const DL_LIST * pEntry) {
    const DL_LIST * pTmp;

    if (pList == pEntry) return true;

    ITERATE_OVER_LIST(pList, pTmp) {
        if (pTmp == pEntry) {
            return true;
        }
    }

    return false;
}

/* safe iterate macro that allows the item to be removed from the list
 * the iteration continues to the next item in the list
 */
#define ITERATE_OVER_LIST_ALLOW_REMOVE(pStart,pItem,st,offset)  \
{                                                       \
    PDL_LIST  pTemp;                                     \
    pTemp = (pStart)->pNext;                            \
    while (pTemp != (pStart)) {                         \
        (pItem) = A_CONTAINING_STRUCT(pTemp,st,offset);   \
         pTemp = pTemp->pNext;                          \

#define ITERATE_IS_VALID(pStart)    DL_ListIsEntryInList(pStart, pTemp)
#define ITERATE_RESET(pStart)       pTemp=(pStart)->pNext

#define ITERATE_END }}

/*
 * DL_ListInsertTail - insert pAdd to the end of the list
*/
static __inline PDL_LIST DL_ListInsertTail(PDL_LIST pList, PDL_LIST pAdd) {
        /* insert at tail */
    pAdd->pPrev = pList->pPrev;
    pAdd->pNext = pList;
    if (pList->pPrev) {
        pList->pPrev->pNext = pAdd;
    }
    pList->pPrev = pAdd;
    return pAdd;
}

/*
 * DL_ListInsertHead - insert pAdd into the head of the list
*/
static __inline PDL_LIST DL_ListInsertHead(PDL_LIST pList, PDL_LIST pAdd) {
        /* insert at head */
    pAdd->pPrev = pList;
    pAdd->pNext = pList->pNext;
    pList->pNext->pPrev = pAdd;
    pList->pNext = pAdd;
    return pAdd;
}

#define DL_ListAdd(pList,pItem) DL_ListInsertHead((pList),(pItem))
/*
 * DL_ListRemove - remove pDel from list
*/
static __inline PDL_LIST DL_ListRemove(PDL_LIST pDel) {
    if (pDel->pNext != NULL) {
        pDel->pNext->pPrev = pDel->pPrev;
    }
    if (pDel->pPrev != NULL) {
        pDel->pPrev->pNext = pDel->pNext;
    }

    /* point back to itself just to be safe, incase remove is called again */
    pDel->pNext = pDel;
    pDel->pPrev = pDel;
    return pDel;
}

/*
 * DL_ListRemoveItemFromHead - get a list item from the head
*/
static __inline PDL_LIST DL_ListRemoveItemFromHead(PDL_LIST pList) {
    PDL_LIST pItem = NULL;
    if (pList->pNext != pList) {
        pItem = pList->pNext;
            /* remove the first item from head */
        DL_ListRemove(pItem);
    }
    return pItem;
}

static __inline PDL_LIST DL_ListRemoveItemFromTail(PDL_LIST pList) {
    PDL_LIST pItem = NULL;
    if (pList->pPrev != pList) {
        pItem = pList->pPrev;
            /* remove the item from tail */
        DL_ListRemove(pItem);
    }
    return pItem;
}

/* transfer src list items to the tail of the destination list */
static __inline void DL_ListTransferItemsToTail(PDL_LIST pDest, PDL_LIST pSrc) {
        /* only concatenate if src is not empty */
    if (!DL_LIST_IS_EMPTY(pSrc)) {
            /* cut out circular list in src and re-attach to end of dest */
        pSrc->pPrev->pNext = pDest;
        pSrc->pNext->pPrev = pDest->pPrev;
        pDest->pPrev->pNext = pSrc->pNext; 
        pDest->pPrev = pSrc->pPrev;
            /* terminate src list, it is now empty */      
        pSrc->pPrev = pSrc;
        pSrc->pNext = pSrc;
    }
}

/* transfer src list items to the head of the destination list */
static __inline void DL_ListTransferItemsToHead(PDL_LIST pDest, PDL_LIST pSrc) {
        /* only concatenate if src is not empty */
    if (!DL_LIST_IS_EMPTY(pSrc)) {
            /* cut out circular list in src and re-attach to start of dest */
        pSrc->pNext->pPrev = pDest;
        pDest->pNext->pPrev = pSrc->pPrev;
        pSrc->pPrev->pNext = pDest->pNext;
        pDest->pNext = pSrc->pNext;
            /* terminate src list, it is now empty */      
        pSrc->pPrev = pSrc;
        pSrc->pNext = pSrc;
    }
}

#endif /* __DL_LIST_H___ */
