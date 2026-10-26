/* Actual directory-state declaration plus production initialization lock.
 * This test opens no game, directory, network socket or audio device. */
#include "../source/tag_files/files_windows.c"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
void platform_playlist_defaults_lock(void);
void platform_playlist_defaults_unlock(void);
static pthread_mutex_t start_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_condition=PTHREAD_COND_INITIALIZER;
static int arrivals;
static int first_time=1,initializations;
static struct find_files_state *states[2];
static void require(int good)
{
    if(!good){fputs("FAIL playlist initialization/thread-local scan state\n",stderr);abort();}
}
static void *worker(void *arg)
{
    unsigned long n=(unsigned long)arg,i;
    states[n]=&find_files_globals;
    require(find_files_globals.depth==NONE);
    require(find_files_globals.handles[0]==INVALID_HANDLE_VALUE);
    find_files_globals.flags=n+111;
    pthread_mutex_lock(&start_lock);
    arrivals++;
    if(arrivals==2)pthread_cond_broadcast(&start_condition);
    while(arrivals<2)pthread_cond_wait(&start_condition,&start_lock);
    pthread_mutex_unlock(&start_lock);
    for(i=0;i<10000;i++){
        require(find_files_globals.flags==n+111);
        platform_playlist_defaults_lock();
        if(first_time){initializations++;first_time=0;}
        platform_playlist_defaults_unlock();
    }
    return NULL;
}
static void round(void)
{
    pthread_t a,b;
    arrivals=0;
    require(!pthread_create(&a,NULL,worker,(void *)0));
    require(!pthread_create(&b,NULL,worker,(void *)1));
    pthread_join(a,NULL);pthread_join(b,NULL);
    require(states[0]!=states[1]);
    require(states[0]!=&find_files_globals && states[1]!=&find_files_globals);
}
int main(void)
{
    round();require(initializations==1 && !first_time);
    /* The lifecycle can request defaults again; this isn't pthread_once. */
    platform_playlist_defaults_lock();first_time=1;platform_playlist_defaults_unlock();
    round();require(initializations==2 && !first_time);
    puts("PASS actual directory state isolated per thread; first-use lock serialized 40000 checks; lifecycle reset allowed");
    return 0;
}
