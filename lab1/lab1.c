#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    int event_id;
} EventData;

#define NUM_EVENTS 3

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int ready = 0;
int done = 0;
EventData* data_ptr = NULL;

void* provider(void* arg)
{
    for (int i = 1; i <= NUM_EVENTS; i++)
    {
        sleep(1);

        pthread_mutex_lock(&lock);

        while (ready == 1)
            pthread_cond_wait(&cond, &lock);

        EventData* new_data = (EventData*)malloc(sizeof(EventData));
        if (!new_data)
        {
            perror("malloc");
            done = 1;
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        new_data->event_id = i;

        data_ptr = new_data;
        ready = 1;

        printf("provided %d\n", new_data->event_id);

        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lock);
    }

    pthread_mutex_lock(&lock);
    while (ready == 1)
        pthread_cond_wait(&cond, &lock);

    done = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);

    return NULL;
}

void* consumer(void* arg)
{
    while (1)
    {
        pthread_mutex_lock(&lock);

        while (ready == 0 && !done)
            pthread_cond_wait(&cond, &lock);

        if (ready == 1)
        {
            EventData* data_to_process = data_ptr;
            data_ptr = NULL;
            ready = 0;

            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&lock);

            if (data_to_process)
            {
                printf("consumed %d\n", data_to_process->event_id);
                free(data_to_process);
            }
        }
        else
        {
            pthread_mutex_unlock(&lock);
            break;
        }
    }

    return NULL;
}


int main(void)
{
    pthread_t prod, cons;

    if (pthread_create(&prod, NULL, provider, NULL) != 0) 
    {
        perror("pthread_create provider");
        return 1;
    }
    if (pthread_create(&cons, NULL, consumer, NULL) != 0)
    {
        perror("pthread_create consumer");
        return 1;
    }

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    return 0;
}
