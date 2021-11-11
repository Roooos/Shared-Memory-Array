#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "C:\Program Files (x86)\Microsoft SDKs\MPI\Include\mpi.h"

pthread_barrier_t barr_id;
pthread_mutex_t lock;

typedef struct data_struct {
    int tid;//thread id
    int n, p;//array dimension, number of threads
    int rem;//remaing no of cols
    int* work;//cols to work on?
    int* succ;//success bool
    double prec;//required precision
    double** read;//read array
    double** write;//write array
} data_struct;

//takes thread data structure as argument and does maths
void *do_maths(void *arg) {
    data_struct *data = (data_struct *)arg;
    int n = data->n;
    int r = data->rem;
    int no_col = n/data->p;
    double prec = data->prec;
    int working;

    int row, col;
    double** temp;
    do {
        //makes sure all threads have the same value of succ and work on each loop
        //also makes sure all thread are on the same loop iteration
        pthread_barrier_wait(&barr_id);
        data->succ[0] = 1;
        data->work[0] = 1;
        pthread_barrier_wait(&barr_id);

        //for columns in given range...
        for (col = no_col * (data->tid); col < no_col * (data->tid + 1); col++) {
            //if not edge of array..
            if (col != 0 && col != n-1) {
                //for each row not top or bottom...
                for (row = 1; row < n-1; row++) {
                    //calculate average
                    data->write[row][col] =
                            (data->read[row+1][col] + data->read[row-1][col] + data->read[row][col+1] +
                             data->read[row][col-1]) / 4;

                    //check for success
                    if (fabs(data->write[row][col] - data->read[row][col]) > prec)
                        data->succ[0] = 0;
                }
            }
        }
        //r > 0 indicates n/p is not an int, ie there are remaining columns
        if(r > 0){
            //locks work (ie whether thread has asked if it can work on the remaining columns)
            pthread_mutex_lock(&lock);
            //assigns this value to "working" which will be unique on each thread regardless of timing
            working = data->work[0];
            //decrements work. only one thread will get strictly positive value
            data->work[0] -= 1;
            pthread_mutex_unlock(&lock);

            //if thread that has positive "working" value
            if(working > 0){
                //for columns in given range...
                for(col = n-r-1; col < n-1;  col++) {
                    //for each row not top or bottom...
                    for (row = 1; row < n - 1; row++) {
                        //calculate average
                        data->write[row][col] =
                                (data->read[row + 1][col] + data->read[row - 1][col] + data->read[row][col + 1] +
                                 data->read[row][col - 1]) / 4;

                        //check for success
                        if (fabs(data->write[row][col] - data->read[row][col]) > prec) {
                            data->succ[0] = 0;
                        }
                    }
                }
            }
        }

        //swaps read and write arrays
        temp = data->read;
        data->read = data->write;
        data->write = temp;

        //waits for all threads before checking for success
        pthread_barrier_wait(&barr_id);
    }while(data->succ[0] == 0);
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[] ) {

    int print = 0;
    int printerr = 0;
    int printsucc = 0;

    //default n, p, and prec values
    int n = 100;
    int p = 8;
    double prec = 0.01;

    //command line arguments
    if(argc == 3){
        sscanf(argv[1],"%d",&n);
        sscanf(argv[2],"%d",&p);
    }
    else if( argc >= 4 ) {
        sscanf(argv[1],"%d",&n);
        sscanf(argv[2],"%d",&p);
        sscanf(argv[3],"%lf",&prec);
    }else {
        printf("Default: n = %d, p = %d, precision = %f\n", n, p, prec);
    }
    //to print or not to print?
    if( argc == 5 ) {
        if(strcmp(argv[4], "prnt") == 0)
            print = 1;
        if(strcmp(argv[4], "succ") == 0)
            printsucc = 1;
        if(strcmp(argv[4], "err") == 0){
            printsucc = 1;
            printerr = 1;
        }
    }
    else if(argc == 6){
        if(strcmp(argv[4], "prnt") == 0 || strcmp(argv[5], "prnt") == 0)
            print = 1;
        if(strcmp(argv[4], "succ") == 0 || strcmp(argv[5], "succ") == 0)
            printsucc = 1;
        if(strcmp(argv[4], "err") == 0 || strcmp(argv[5], "err") == 0){
            printsucc = 1;
            printerr = 1;
        }
    }else if(argc == 7){
        print = 1;
        printsucc = 1;
        printerr = 1;
    }

    //no point in having more threads than columns
    if(p>n)
        p = n;

    printf("size of array: %d\n", n);
    printf("number of processors: %d\n", p);
    printf("precision: %f\n", prec);

    //how many columns will each thread work on
    int c = n/p;
    //number remaining columns not accounted for (ignoring edge case)
    int r = n-(c*p)-1;

    int i, j;

    //assigns memory for array of dimension n
    double* arr_single = calloc(n*n, sizeof(double));
    double** arr = malloc(sizeof(double) * n);
    for (i = 0; i < n; ++i)
        arr[i] = arr_single + i*n;
    //assigns random value to each element
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            arr[i][j] = (rand() % 39) + 10;
        }
    }

    //assign space for read and write arrays
    double* warr_single = calloc(n*n, sizeof(double));
    double** warr = malloc(sizeof(double) * n);
    double* rarr_single = calloc(n*n, sizeof(double));
    double** rarr = malloc(sizeof(double) * n);
    for (i = 0; i < n; ++i) {
        warr[i] = warr_single + i * n;
        rarr[i] = rarr_single + i * n;
        for (j = 0; j < n; j++) {
            warr[i][j] = arr[i][j];
            rarr[i][j] = arr[i][j];
        }
    }

    int* success = calloc(1, sizeof(int));
    success[0] = 1;

    int* work = calloc(1, sizeof(int));
    work[0] = 1;

    int rc;
    pthread_t thr[p];
    data_struct thr_data[p];
    pthread_barrier_init(&barr_id,NULL,p);

    int msec = 0;
    clock_t before = clock();

    //create threads
    for (i = 0; i < p; ++i) {
        thr_data[i].tid = i;
        thr_data[i].n = n;
        thr_data[i].p = p;
        thr_data[i].prec = prec;
        thr_data[i].write = warr;
        thr_data[i].read = rarr;
        thr_data[i].succ = success;
        thr_data[i].rem = r;
        thr_data[i].work = work;

        if ((rc = pthread_create(&thr[i], NULL, do_maths, &thr_data[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return EXIT_FAILURE;
        }
    }

    //wait for all threads to complete
    for (i = 0; i < p; ++i) {
        pthread_join(thr[i], NULL);
    }

    clock_t difference = clock() - before;
    msec = difference * 1000 / CLOCKS_PER_SEC;
    printf("Time taken %d milliseconds\n", msec);

    //prints final array if asked in cmd
    if(print) {
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                printf("%f ", rarr[i][j]);
            }
            printf("\n");
        }
    }

    //prints success state if requested in cmd
    if(printsucc){
        //success test
        int successful = 1;
        for (i = 0; i < n; i++) {
            for (j = 0; j < n; j++) {
                //edge case
                if (i == 0 || i == n - 1 || j == 0 || j == n - 1) {
                    if (warr[i][j] - rarr[i][j] == 0) {
                        warr[i][j] = 1;
                    } else
                        warr[i][j] = 0;
                //rest of array
                } else if (
                        fabs(rarr[i][j] - ((rarr[i + 1][j] + rarr[i - 1][j] + rarr[i][j + 1] + rarr[i][j - 1]) / 4))
                        > prec) {
                    successful = 0;
                    warr[i][j] = 0;
                    printf("Failed at [%d][%d] with difference %f\n", i, j,
                           rarr[i][j] - ((rarr[i + 1][j] + rarr[i - 1][j] + rarr[i][j + 1] + rarr[i][j - 1]) / 4));
                } else {
                    warr[i][j] = 1;
                }
            }
        }
        printf("success: %d", successful);

        //if print error is requested in cmd array where 0 indicates failure
        if(printerr) {
            for (i = 0; i < n; i++) {
                for (j = 0; j < n; j++) {
                    printf("%.f ", warr[i][j]);
                }
                printf("\n");
            }
        }

    }
    return EXIT_SUCCESS;
 }