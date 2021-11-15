#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

#ifndef QSIZE
#define QSIZE 8
#endif

typedef struct {
	char data[250];//lets say the file name for now
	unsigned count;
	unsigned head;
	unsigned activeThreads;
	int open;
	pthread_mutex_t lock;
	pthread_cond_t read_ready;
	pthread_cond_t write_ready;
} queue_t;

int init(queue_t *Q)
{
	Q->activeThreads = 0;
	Q->count = 0;
	Q->head = 0;
	Q->open = 1;
	pthread_mutex_init(&Q->lock, NULL);
	pthread_cond_init(&Q->read_ready, NULL);
	pthread_cond_init(&Q->write_ready, NULL);
	
	return 0;
}

int destroy(queue_t *Q)
{
	pthread_mutex_destroy(&Q->lock);
	pthread_cond_destroy(&Q->read_ready);
	pthread_cond_destroy(&Q->write_ready);

	return 0;
}

int isDir(const char *path) { //0 if file, != 0 if dir
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

struct comp_result{
	char file1[250];
	char file2[250];
	unsigned tokens; //word count of file1+file2
	double distance; //JSD b/t file 1 and file2
};
struct words_t{
	char word[250];//the actual word
	int freq;//freq of the word
	struct words_t* next;//ptr to next word

	int f1;
	int f2;
};
struct files_t{
	char fileName[250];//the file name
	struct words_t* listOfWords;//ptr to list of words for this file
	struct files_t* next;//ptr to next file
	int test;//total # of words in this file
	pthread_mutex_t lock;
	pthread_cond_t read_ready;
	pthread_cond_t write_ready;
};
int initialize(struct files_t *F){
	F->listOfWords = NULL;
	F->test = 0;
	F->next = NULL;

	return 0;
}
int destruction(struct files_t *F){
	pthread_mutex_destroy(&F->lock);
	pthread_cond_destroy(&F->read_ready);
	pthread_cond_destroy(&F->write_ready);

	return 0;
}

// add item to end of queue
// if the queue is full, block until space becomes available
int enqueue(queue_t *Q, char *item)
{
	pthread_mutex_lock(&Q->lock);
	
	while (Q->count == QSIZE && Q->open) {
		pthread_cond_wait(&Q->write_ready, &Q->lock);
	}
	if (!Q->open) {
		pthread_mutex_unlock(&Q->lock);
		return -1;
	}
	unsigned i = Q->head + Q->count;
	if (i >= QSIZE) i -= QSIZE;
	//printf("enqueud: %s\n", item);
	strcpy(Q->data, item);
	++Q->count;
	
	pthread_cond_signal(&Q->read_ready);
	
	pthread_mutex_unlock(&Q->lock);
	
	return 0;
}

int dequeue(queue_t *Q, char *item){
	pthread_mutex_lock(&Q->lock);
	
	if(Q->count == 0){
		--Q->activeThreads;
		if(Q->activeThreads == 0){
			pthread_mutex_unlock(&Q->lock);
			pthread_cond_broadcast(&Q->read_ready);
			pthread_cond_broadcast(&Q->write_ready);
			return -1;
		}
		while(Q->count == 0 && Q->open && Q->activeThreads > 0){
			pthread_cond_wait(&Q->read_ready, &Q->lock);
		}
		if(Q->count == 0){
			pthread_mutex_unlock(&Q->lock);
			return -1;
		}
		++Q->activeThreads;
	}
	strcpy(item, Q->data);
	--Q->count;
	++Q->head;
	if (Q->head == QSIZE) Q->head = 0;
	
	pthread_cond_signal(&Q->write_ready);
	
	pthread_mutex_unlock(&Q->lock);
	
	return 0;
}

int qclose(queue_t *Q)
{
	pthread_mutex_lock(&Q->lock);
	Q->open = 0;
	pthread_cond_broadcast(&Q->read_ready);
	pthread_cond_broadcast(&Q->write_ready);
	pthread_mutex_unlock(&Q->lock);	

	return 0;
}

struct targs { //data structure that holds the file and the wfd and stuff
	queue_t *Q;
	queue_t *QD;
	char id[250];//name of the file?
	int max;
	int wait;
	int total;//total # of words
	struct files_t *head;//ptr to the LL of files
	//the following attributes are added for anaylsis phase
	int totalComparisons; // = numfiles*(numfiles-1)/2
	int threadNumber;
	int dividend; //dividend = totalComparisons / # of threads
	struct comp_result *results;
};

void *producer(void *A) 
{
	struct targs *args = A;
	char j[250];
	//printf("begin producing\n");
	while(1){
		for(int i = 0; i < args->max; ++i){
			if(dequeue(args->QD, j) != 0){
				printf("Dir Dequeue failed\n");
				return NULL;
			}
		}
		DIR *dirp = opendir(j);
		if(dirp != NULL){//its a directory
			struct dirent *de;
			while((de = readdir(dirp))){
				if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0){ continue; }
				if(de->d_name[0] == '.'){ continue; }
				char chdir[250];
				strcat(j, "/");
				strcat(j, de->d_name);
				strcpy(chdir, j);
				if(isDir(chdir) == 0){//its a regular file, add to file queue
					struct files_t *newNode = malloc(sizeof(struct files_t));
					strcpy(newNode -> fileName, j);
					newNode->next = args->head->next;
					newNode->test = 0;
					newNode->listOfWords = NULL;
					args->head->next=newNode;
					if(enqueue(args->Q, chdir) != 0){
						printf("File Enqueue failed\n");
						return NULL;
					};
				}
				else if(isDir(chdir) != 0){//its a directory, add to directory queue
					if(enqueue(args->QD, chdir) != 0){
						printf("Dir Enqueue Failed\n");
						return NULL;
					}
				}
			}
		}
		closedir(dirp);
		if(args->QD->count == 0 && args->QD->activeThreads == 0){
			return NULL;
		}
	}
	return NULL;
}

void *read_file(void *A){
	struct targs *args = A;
	char j[250];

	//printf("begin consuming\n");
	while(1){
		if (dequeue(args->Q, j) != 0) {
			printf("Dequeue failed\n");
			return NULL;
		}
		
		FILE *f = fopen(j, "r");
		char x[1024];
		while(fscanf(f, " %1023s", x) == 1){
			//x has our word, now see if the file has that word
			struct files_t* ptr = args->head;
			while(ptr != NULL){
				if(strcmp(ptr->fileName, j) == 0){
					//ptr is now the file_t that we want
					break;
				}
				ptr = ptr -> next;
			}
			//now we want to find the word in the ptr->listOfWords LL. (ptr->listOfWords->word)
			
			struct words_t* wordPtr = ptr->listOfWords;
			struct words_t* prev = 0;
			if(ptr->test==0){
				struct words_t* newWord = malloc(sizeof(struct words_t));
				strcpy(newWord->word, x);
				newWord->freq = 1;
				newWord->next = NULL;
				ptr->listOfWords = newWord;
				++ptr->test;
				continue;
			}
			int wordFound = 0;
			while(wordPtr != NULL){
				if(strcmp(wordPtr->word, x) == 0){
					//this is the word we want to increase
					++wordPtr->freq;
					++ptr->test;
					wordFound = 1;
					break;
				}
				prev = wordPtr;
				wordPtr = wordPtr->next;
			}
			//make a new word prev->next
			if(wordFound == 0){
				struct words_t* newWord = malloc(sizeof(struct words_t));
				strcpy(newWord->word, x);
				newWord->freq = 1;
				newWord->next = NULL;
				prev->next = newWord;
				++ptr->test;
			}
		}
		fclose(f);

		//printf("Received: [%s]\n", j);
		sleep(1);
		if(args->Q->count == 0){// && args->QD->count == 0){ prof sudocode says to use this, but when i dont it works?...
			return NULL;
		}
	}
	//printf("finished\n");
	return NULL;	
}
double compute_jsd(struct files_t *f1, struct files_t *f2){//f1->listOfWords->word
	double jsd = 0.0;
	//compute the jsd here
	struct words_t *total = malloc(sizeof(struct words_t));
	total->next = NULL;

	//traverse f1
	struct words_t *f1w = f1->listOfWords->next;
	while(f1w != NULL){
		char w[250];
		strcpy(w, f1w->word);
		struct words_t *ptr = total;
		struct words_t *prev = NULL;
		int found = 0;
		while(ptr != NULL){
			if(strcmp(w, ptr->word) == 0){
				ptr->f1 = f1w->freq;
				ptr->freq += f1w->freq;
				found = 1;
				break;
			}
			prev = ptr;
			ptr = ptr->next;
		}
		if(found == 0){// add word
			struct words_t *addW = malloc(sizeof(struct words_t));
			addW->next = NULL;
			strcpy(addW->word, w);
			addW->freq = f1w->freq;	
			prev->next = addW;
		}
		f1w = f1w->next;
	}
	struct words_t *f2w = f2->listOfWords->next;
	while(f2w != NULL){
		char w[250];
		strcpy(w, f2w->word);
		struct words_t *ptr = total;
		struct words_t *prev = NULL;
		int found = 0;
		while(ptr != NULL){
			if(strcmp(w, ptr->word) == 0){
				ptr->f2 = f2w->freq;
				ptr->freq += f2w->freq;
				found = 1;
				break;
			}
			prev = ptr;
			ptr = ptr->next;
		}
		if(found == 0){// add word
			struct words_t *addW = malloc(sizeof(struct words_t));
			addW->next = NULL;
			strcpy(addW->word, w);
			addW->freq = f2w->freq;	
			prev->next = addW;
		}
		f2w = f2w->next;
	}

	double KLD1 = 0;
	double KLD2 = 0;
	double log1, log2;
	double WFD1, WFD2;
	struct words_t *ptr = total;
	while(ptr != NULL){
		WFD1 = 0;
		WFD2 = 0;
		log1 = 0;
		log2 = 0;
		WFD1 = (ptr->f1)/f1->test;
		log1 = WFD1/((ptr->freq)/(f1->test + f2->test));
		//printf("%d, %d, %d", ptr->freq, f1->test, f2->test);
		KLD1 += WFD1 * (log10(log1)/log10(2));
		
		WFD2 = (ptr->f2)/f2->test;
		log2 = WFD2/((ptr->freq)/(f2->test + f2->test));
		KLD2 += WFD2 * (log10(log2)/log10(2));
		ptr = ptr->next;
	}
	//jsd = sqrt((0.5 * KLD1) + (0.5 * KLD2)); this is correct 
	jsd = log10(WFD1*WFD2 * -1);

	struct words_t *freeW = total;
	while(freeW != 0){
		struct words_t* hold = freeW;
		freeW = freeW->next;
		free(hold);
	}
	return jsd;
}
void *analyze(void *A){//in this case, we want to refer to files_t, the LL of files we traversed/will analyze
	struct targs *args = A;
	
	struct comp_result *computations = args->results;
	int i = args->threadNumber * args->dividend;
	struct files_t *filePtr = args->head->next;
	struct files_t *file2Ptr = filePtr->next;
	for(int f1 = args->threadNumber * args->dividend; f1 < ((args->threadNumber) + 1) * args->dividend; f1++){
		for(int f2 = f1+1; f2 < (((args->threadNumber) + 1) * args->dividend) + 1; f2++){
			strcpy(computations[i].file1, filePtr->fileName);
			strcpy(computations[i].file2, file2Ptr->fileName);
			computations[i].tokens = filePtr->test + file2Ptr->test;
			computations[i].distance = compute_jsd(filePtr, file2Ptr);
			++i;
			file2Ptr = file2Ptr->next;
		}
		filePtr = filePtr->next;
	}
	return NULL;
}
int main(int argc, char **argv)
{
	//read options
	char f1[250];
	char f2[250];
	strcpy(f1, argv[1]);
	strcpy(f2, argv[2]);
	unsigned dirThreads, fThreads, aThreads;//, suffix;
	dirThreads = 1;
	fThreads = 1;
	aThreads = 1;
	//suffix = ".txt";

	//initialize queues
	queue_t Q; //file queue
	queue_t QD; //directory queue
	init(&Q);
	init(&QD);
	//initialize the LL of FILES
	struct files_t *listOfFiles = malloc(sizeof(struct files_t));
	initialize(listOfFiles);

	unsigned threads;
	threads = dirThreads + fThreads + aThreads;

	struct targs *args;//arraylist of all the file data structure things
	pthread_t *tids;
	args = malloc(threads * sizeof(struct targs));
	tids = malloc(threads * sizeof(pthread_t));

	//start file/directory threads
	unsigned i;
	for(i = 0; i < fThreads; i++){
		args[i].Q = &Q;
		strcpy(args[i].id, "");//file name
		args[i].max = 1;
		args[i].head = listOfFiles;
		args[i].head->listOfWords = NULL;
		pthread_create(&tids[i], NULL, read_file, &args[i]);
	}
	for(; i < fThreads+dirThreads; i++){
		args[i].Q = &Q;
		args[i].QD = &QD;
		strcpy(args[i].id, "");//file name
		args[i].max = 1;
		args[i].head = listOfFiles;
		args[i].head->listOfWords = NULL;
		pthread_create(&tids[i], NULL, producer, &args[i]);
	}
	char testFile[250];
	strcpy(testFile, f1);
	strcat(testFile, ".txt");
	if(isDir(testFile) == 0 && fopen(testFile, "r") != NULL){ //regular file, enqueue to regular queue
		struct files_t *newNode = malloc(sizeof(struct files_t));
		strcat(f1, ".txt");
		strcpy(newNode -> fileName, f1);
		newNode->next = listOfFiles->next;
		newNode->test = 0;
		newNode->listOfWords = NULL;
		listOfFiles->next=newNode;
		enqueue(&Q, f1);
	}
	else{
		enqueue(&QD, f1);
	}
	char test2File[250];
	strcpy(test2File, f2);
	strcat(test2File, ".txt");
	if(isDir(test2File) == 0 && fopen(test2File, "r") != NULL){
		struct files_t *newNode = malloc(sizeof(struct files_t));
		strcat(f2, ".txt");
		strcpy(newNode -> fileName, f2);
		newNode->next = listOfFiles->next;
		newNode->test = 0;
		newNode->listOfWords = NULL;
		listOfFiles->next=newNode;
		enqueue(&Q, f2);
	}
	else{
		enqueue(&QD, f2);
	}

	sleep(1);
	qclose(&Q);
	qclose(&QD);
	//printf("Queue closed\n");
	//close the file and directory threads (we still have to access analysis threads
	for(i = 0; i < fThreads+dirThreads; ++i){
		pthread_join(tids[i], NULL);
	}
	//beginning phase 2, analysis threads. our data should be stored
	int threadCount = 0;
	int numFiles = 0;
	struct files_t *getNumFilesPtr = args->head->next;
	while(getNumFilesPtr != 0){
		numFiles++;
		getNumFilesPtr = getNumFilesPtr -> next;
	}
	//printf("le numer o files:%d\n", numFiles);
	int numComps = numFiles*(numFiles-1)/2;
	struct targs *args2;
	args2 = malloc(aThreads * sizeof(struct targs));
	
	for(i = fThreads+dirThreads; i < threads; i++){
		struct files_t *pt = args->head->next;
		struct files_t *add1 = malloc(sizeof(struct files_t));//base
		add1->listOfWords = NULL;
		struct files_t *scndPtr = add1;
		while(pt != 0){
			struct files_t *fileToCopy = malloc(sizeof(struct files_t));
			strcpy(fileToCopy->fileName, pt->fileName);
			fileToCopy->listOfWords = NULL;
			fileToCopy->test = pt->test;	

			struct words_t *wordPt = pt->listOfWords;
			struct words_t *add2 = malloc(sizeof(struct words_t));//base of words
			struct words_t *thdPtr = add2;
			while(wordPt != NULL){
				struct words_t *wordToCopy = malloc(sizeof(struct words_t));
				strcpy(wordToCopy->word, wordPt->word);
				wordToCopy->freq = wordPt->freq;
				wordToCopy->next = NULL;
				thdPtr->next = wordToCopy;
				thdPtr = thdPtr->next;
				wordPt = wordPt->next;
			}
			fileToCopy->listOfWords = add2;
			fileToCopy->next = NULL;
			pt = pt->next;
			scndPtr->next = fileToCopy;
			scndPtr = scndPtr->next;
		}	
		args2[threadCount].head = add1;	
		args2[threadCount].results = malloc(numComps*sizeof(struct comp_result));
		args2[threadCount].threadNumber = threadCount;
		args2[threadCount].dividend = numComps / aThreads;
		args2[threadCount].totalComparisons = numComps;
		//printf("a:%s\n", args->head->next->fileName);
		pthread_create(&tids[i], NULL, analyze, &args2[threadCount]);
		threadCount++;
	}
	sleep(1);
	//printf("Analysis completed\n");
	for(i = fThreads+dirThreads; i < threads; ++i){
		pthread_join(tids[i],NULL);
	}
	//print the repo of words
	/*struct files_t *ptrr = args->head->next;
	while(ptrr != 0){
		struct words_t *wordPtr = ptrr->listOfWords;
		printf("[%s]\n", ptrr->fileName);
		while(wordPtr != NULL){
			printf("%s: %d\n", wordPtr->word, wordPtr->freq);
			wordPtr = wordPtr->next;
		}
		ptrr = ptrr->next;
	}*/
	//printf("Printing the Repo\n");
	for(int r = 0; r < numComps; r++){
		printf("[%s]<--->[%s]: %lf\n", args2->results[r].file1, args2->results[r].file2, args2->results[r].distance);
		//printf("[%s]<--->[%s]\n", args2->results[r].file1, args2->results[r].file2);
	}


	destroy(&Q);
	destroy(&QD);
	struct files_t *ptr = args->head;
	while(ptr != 0){
		struct files_t *temp = ptr;
		//also clear the words of that file
		//destruction(ptr);
		struct words_t *findWords = ptr->listOfWords;
		while(findWords != NULL){
			struct words_t* hold = findWords;
			findWords = findWords->next;
			free(hold);
		}
		ptr = ptr->next;
		free(temp);
	}
	struct files_t *lastPtr = args2->head;
	while(lastPtr != 0){
		struct files_t *temp = lastPtr;
		struct words_t *findWords = lastPtr->listOfWords;
		while(findWords != NULL){
			struct words_t* hold = findWords;
			findWords = findWords->next;
			free(hold);
		}
		lastPtr = lastPtr->next;
		free(temp);
		
	}
	//freeArgs(args->head);
	//freeArgs(args2->head);
	free(args2->results);

	free(args2);
	free(args);
	free(tids);
	return EXIT_SUCCESS;
}
