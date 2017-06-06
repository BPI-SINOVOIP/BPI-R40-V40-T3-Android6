/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : awMessageQueue.h
 * Description : MessageQueue
 * History :
 *
 */


#ifndef AW_MESSAGE_QUEUE_H
#define AW_MESSAGE_QUEUE_H

#include <semaphore.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

//typedef unsigned long        uintptr_t;

#ifndef uintptr_t
typedef size_t uintptr_t;
#endif

typedef void* AwMessageQueue;

typedef struct AwMessage AwMessage;

struct AwMessage
{
    int          messageId;
    uintptr_t     params[8];
};

AwMessageQueue* AwMessageQueueCreate(int nMaxMessageNum);

void AwMessageQueueDestroy(AwMessageQueue* mq);

int AwMessageQueuePostMessage(AwMessageQueue* mq, AwMessage* m);

int AwMessageQueueGetMessage(AwMessageQueue* mq, AwMessage* m);

int AwMessageQueueTryGetMessage(AwMessageQueue* mq, AwMessage* m, int64_t timeout);

int AwMessageQueueFlush(AwMessageQueue* mq);

int AwMessageQueueGetCount(AwMessageQueue* mq);

//* define a semaphore timedwait method for common use.
int SemTimedWait(sem_t* sem, int64_t time_ms);

void setMessage(AwMessage* msg,
                int        cmd,
                uintptr_t        param0 = 0,
                uintptr_t        param1 = 0,
                uintptr_t        param2 = 0,
                uintptr_t        param3 = 0,
                uintptr_t        param4 = 0,
                uintptr_t        param5 = 0,
                uintptr_t        param6 = 0,
                uintptr_t        param7 = 0);

#endif

