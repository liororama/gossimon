#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *work(void *ptr) {
    int a;
    while (1) {
        a=2;
    }
}

int main(int argc, char** argv) {
    int num_threads, t;
    pthread_t **threads;
    if (argc >= 2) {
        num_threads = atoi(argv[1]);
    } else {
        num_threads = 1;
    }
    printf ("Running %d threads...\n",num_threads);
    num_threads--;
    threads = (pthread_t**)malloc(sizeof(pthread_t*)*num_threads);
    
    // create threads
    for (;num_threads > 0; num_threads--) {
        threads[num_threads-1] = (pthread_t*)malloc(sizeof(pthread_t));
        t = pthread_create( threads[num_threads-1], NULL, work, (void *)NULL);
        pthread_detach (threads[num_threads-1]);
    }

    // we're the last thread!
    work(NULL);
}
