
#ifndef INVOKE_H_
#define INVOKE_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dpiheader.h"

#define PCIe_Speed	100

typedef enum STATE { RESET, LOAD, READY, RUN, POLL, STOP, ERROR } STATE;

typedef struct TaskList {
	pid_t id;
	size_t in_size;
	size_t out_size;
	char * data;
	int cursor;
	int state;
} TaskList;


int task_create(pid_t id, size_t size1, size_t size2);

STATE task_state(int taskid);

int task_setdata(int taskid, char * ptr, size_t size);

int task_getdata(int taskid, char * ptr, size_t size);

#endif


