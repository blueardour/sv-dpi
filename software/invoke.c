
#include "invoke.h"

extern int _debug;

static struct TaskList tasks [256];
static int taskid = 0;

/******************  scheduler ***********************/
int scheduler_once() // should always return 0
{
	int i;

	// check busy
	for(i=0; i<256; i++) if(tasks[i].state == RUN) return 0;
	
	// choose next READY task
//	for(i=0; i<256; i++)
//		if(tasks[i].state == READY) {
//			tasks[i].state = RUN;
//			dpi_setstate(RUN);
//			kernel_kmeans(i);
//			break;
//		}
	return 0;
}

/******************  Data transfer ***********************/
int task_create(pid_t id, size_t size1, size_t size2)
{
	tasks[taskid].id = id;
	tasks[taskid].in_size= size1;
	tasks[taskid].out_size = size2;
	tasks[taskid].data = (char *) malloc(size1 + size2);
	tasks[taskid].cursor = 0;
	tasks[taskid].state = RESET;
	taskid++;

	dpi_delay((sizeof(pid_t) + sizeof(size_t) * 2) * PCIe_Speed);
	return taskid - 1;
}

int task_setdata(int taskid, char * ptr, size_t size)
{
	strncpy(tasks[taskid].data + tasks[taskid].cursor, ptr, size);
	tasks[taskid].cursor += size;

	if(tasks[taskid].cursor >= tasks[taskid].in_size) tasks[taskid].state = READY;
	else tasks[taskid].state = LOAD;

	dpi_delay(size * PCIe_Speed);
	return 0;
}

int task_getdata(int taskid, char * ptr, size_t size)
{
	strncpy(ptr, tasks[taskid].data + tasks[taskid].cursor, size);
	tasks[taskid].cursor += size;

	dpi_delay(size * PCIe_Speed);
	return 0;
}

STATE task_state(int taskid)
{
	dpi_delay(sizeof(STATE) * PCIe_Speed);
	return tasks[taskid].state;
}

/*****************************************/

__inline static float euclid_dist_2(int, float *, float *);
__inline static int find_nearest_cluster(int, int, float *, float **);

void kernel_kmeans(int taskid)
{
	float **objects;      /* in: [numObjs][numCoords] */
	int     numCoords;    /* no. features */
	int     numObjs;      /* no. objects */
	int     numClusters;  /* no. clusters */
	float   threshold;    /* % objects change membership */

	int    *membership;   /* out: [numObjs] */
	int    *iterations;
	float  **clusters;

	float delta;       /* % of objects change their clusters */
	int index, loop;
	float  **newClusters;    /* [numClusters][numCoords] */
	int     *newClusterSize; /* [numClusters]: no. objects assigned in each
				    new cluster */

	int cursor;
	int i, j;

	if(_debug) printf("enter kernel \n");

	if(tasks[taskid].data == NULL) goto bad_para;

	cursor = 0;
	numCoords = *(int *)(tasks[taskid].data + cursor);
	cursor += sizeof(int);
	//if(_debug) printf("  numCoords: %d \n", numCoords);

	numObjs = *(int *)(tasks[taskid].data + cursor);
	cursor += sizeof(int);

	numClusters = *(int *)(tasks[taskid].data + cursor);
	cursor += sizeof(int);

	threshold = *(float *)(tasks[taskid].data + cursor);
	cursor += sizeof(float);

	clusters = (float**) malloc(numClusters * sizeof(float*));
	objects    = (float**)malloc(numObjs * sizeof(float*));
	if(objects == NULL || clusters == NULL) goto mem1;

	objects[0] = (float *)(tasks[taskid].data + cursor);
	cursor += sizeof(float) * numObjs * numCoords;

	if(cursor != tasks[taskid].in_size) goto bad_para;

	iterations = (int *)(tasks[taskid].data + cursor);
	cursor += sizeof(int);
	membership = (int *) (tasks[taskid].data + cursor);
	cursor += sizeof(int) * numObjs;
	clusters[0] = (float *) (tasks[taskid].data + cursor);

	if(_debug) printf("  read parameter \n");

	for (i=1; i<numClusters; i++) clusters[i] = clusters[i-1] + numCoords;
	for (i=1; i<numObjs; i++) objects[i] = objects[i-1] + numCoords;

	/*****************************************************************/

	/* initialize membership[] */
	for (i=0; i<numObjs; i++) membership[i] = -1;

	/* need to initialize newClusterSize and newClusters[0] to all 0 */
	newClusterSize = (int*) calloc(numClusters, sizeof(int));
	newClusters    = (float**) malloc(numClusters * sizeof(float*));
	if(newClusters == NULL || newClusterSize == NULL) goto mem2;

	newClusters[0] = (float*)  calloc(numClusters * numCoords, sizeof(float));
	if(newClusters[0] == NULL) goto mem3;

	for (i=1; i<numClusters; i++)
		newClusters[i] = newClusters[i-1] + numCoords;

	if(_debug) printf("  initial memory space \n");

	loop = 0;
	do {
		delta = 0.0;
		for (i=0; i<numObjs; i++) {
			/* find the array index of nestest cluster center */
			index = find_nearest_cluster(numClusters, numCoords, objects[i],
					clusters);

			/* if membership changes, increase delta by 1 */
			if (membership[i] != index) delta += 1.0;

			/* assign the membership to object i */
			membership[i] = index;

			/* update new cluster centers : sum of objects located within */
			newClusterSize[index]++;
			for (j=0; j<numCoords; j++)
				newClusters[index][j] += objects[i][j];
		}

		/* average the sum and replace old cluster centers with newClusters */
		for (i=0; i<numClusters; i++) {
			for (j=0; j<numCoords; j++) {
				if (newClusterSize[i] > 0)
					clusters[i][j] = newClusters[i][j] / newClusterSize[i];
				newClusters[i][j] = 0.0;   /* set back to 0 */
			}
			newClusterSize[i] = 0;   /* set back to 0 */
		}

		delta /= numObjs;
	} while (delta > threshold && loop++ < 500);

	*iterations = loop + 1;

	if(_debug) printf("  done \n");

	free(newClusters[0]);
        free(newClusters);
        free(newClusterSize);
	free(clusters);
	free(objects);

	tasks[taskid].state = STOP;
	goto finish;

mem3:
	if(_debug) printf("error: mem3 \n");
	if(newClusters[0] != NULL) free(newClusters[0]);
mem2:
	if(_debug) printf("error: mem2 \n");
        if(newClusters != NULL) free(newClusters);
        if(newClusterSize != NULL) free(newClusterSize);
mem1:
	if(_debug) printf("error: mem1 \n");
	if(clusters != NULL) free(clusters);
	if(objects != NULL) free(objects);
 
bad_para:	
	if(_debug) printf("error: bad para \n");
	tasks[taskid].state = ERROR;
finish:
	// simulate real IPcore delay
	dpi_delay(100 * loop * numObjs * numCoords * numClusters);
	;
}


/*----< euclid_dist_2() >----------------------------------------------------*/
/* square of Euclid distance between two multi-dimensional points            */
__inline static
float euclid_dist_2(int numdims,  /* no. dimensions */
		float *coord1,   /* [numdims] */
		float *coord2)   /* [numdims] */
{
	int i;
	float ans=0.0;

	for (i=0; i<numdims; i++)
		ans += (coord1[i]-coord2[i]) * (coord1[i]-coord2[i]);

	return(ans);
}

/*----< find_nearest_cluster() >---------------------------------------------*/
__inline static
int find_nearest_cluster(int numClusters, /* no. clusters */
		int     numCoords,   /* no. coordinates */
		float  *object,      /* [numCoords] */
		float **clusters)    /* [numClusters][numCoords] */
{
	int   index, i;
	float dist, min_dist;

	/* find the cluster id that has min distance to object */
	index    = 0;
	min_dist = euclid_dist_2(numCoords, object, clusters[0]);

	for (i=1; i<numClusters; i++) {
		dist = euclid_dist_2(numCoords, object, clusters[i]);
		/* no need square root */
		if (dist < min_dist) { /* find the min and its array index */
			min_dist = dist;
			index    = i;
		}
	}
	return(index);
}

/*********************************************************/
int main_call_from_sv(int argc, const char * cmd)
{
	char buffer[256], * ptr;
	char ** argv;
	int i, j, rs;

	if(argc < 1 || argc > 256) return -1;

	rs = 0;
	memset(buffer, 0, 256);
	argv = (char **)malloc(argc * sizeof(char*));
	argv[0] = buffer;
	ptr = (char *)cmd;

	for(i=0; i<argc; i++)
	{
		//printf("%s \n", ptr);
		j = 0;
		while(ptr[j] != ' ' && ptr[j] != 0) j++;

		strncpy(argv[i], ptr, j);
		argv[i][j] = 0;
		if(i<argc-1) argv[i+1] = argv[i] + j + 1;

		while(ptr[j] == ' ') j++;
		if(ptr[j] == 0) break;

		ptr = ptr + j;
	}

#ifdef TEST
	printf("%d: ", argc);
	for(i=0; i<argc; i++) printf("%s ", argv[i]);
	printf("\n");
#else
	printf("[main_call_from_sv] call real main\n");
	rs = dpi_main(argc, argv);
#endif
	free(argv);
	return rs;

}
#ifdef TEST
int main(int argc, char ** argv)
{
	printf("hello,world \n");
	return 0;
}
#endif

#ifdef TEST

int main(int argc, char **argv)
{
	char buffer[256];
	int i, rs;
	rs =0;

	memset(buffer, 0, 256);

	for(i=0; i<argc; i++)
	{
		strcat(buffer, argv[i]);
		strcat(buffer, " ");
	}

	//printf("cmd: %s\n", buffer);
	rs = main_call_from_sv(argc, buffer);
	return rs;
}

#endif




