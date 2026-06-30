/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                        *.c cmd node source code                              **
*********************************************************************************/


#include "cmdnode.h"


cmdnode_t*    cmdnode_alloc(uint32_t ackNum, uint32_t sessionID, uint64_t timeStamp, struct proc_obj *proc) {
    cmdnode_t* cnode = (cmdnode_t*)vmalloc(sizeof(cmdnode_t));
    if (cnode == NULL) {
        printk("ERROR:cmdnode alloc failed\n");
        return NULL;
    }

    cnode->ackNum    = ackNum;
    cnode->sessionID = sessionID;
    cnode->timeStamp = timeStamp;
    cnode->procObj   = (uint64_t)proc;
    cnode->proc      = proc;
    cnode->code      = CMD_ERR_UNKNOWN;
	init_waitqueue_head(&cnode->wait);
    kref_init(&cnode->refcount);//refcount 1
    return cnode;
}

int32_t       cmdnode_insert(struct rb_root *root, cmdnode_t *cnode) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    /* Figure out where to put new node */
    while (*new) {
        cmdnode_t *this = container_of(*new, cmdnode_t, node);

        parent = *new;
        if (cnode->ackNum < this->ackNum) {
            new = &((*new)->rb_left);
        } else if (cnode->ackNum > this->ackNum) {
            new = &((*new)->rb_right);
        } else {  
            return 0; 
        }
    }

    /* Add new node and rebalance tree. */
    rb_link_node(&cnode->node, parent, new);
    rb_insert_color(&cnode->node, root);
    return 1;
}

cmdnode_t*    cmdnode_search(struct rb_root *root, uint32_t ackNum) {
    struct rb_node *node = root->rb_node;

    while (node) {
        cmdnode_t *cnode = container_of(node, cmdnode_t, node);

        if (ackNum < cnode->ackNum) {
            node = node->rb_left;
        } else if (ackNum > cnode->ackNum) {
            node = node->rb_right;
        } else {
            return cnode;
        }
    }

    return NULL;
}

void           cmdnode_delete(struct rb_root *root, cmdnode_t *cnode) {
    rb_erase(&cnode->node, root);
}

static   void cmdnode_release(struct kref *ref) {
    cmdnode_t *cnode = container_of(ref, cmdnode_t, refcount);
    vfree(cnode);
}

void          cmdnode_free(cmdnode_t *cnode) {
    kref_put(&cnode->refcount, cmdnode_release);
}