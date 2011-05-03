#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef struct {
  int var1;
  int total;
} mydata_t;

void *thread_function(void *ptr)
{
  mydata_t *data = (mydata_t*)ptr;
  int i;
  
  for(i=0; i<10000000; ++i) {
    data->total = data->var1++;
  }
  return NULL;
}

int main(int argc, char **argv)
{
  pthread_t thread1, thread2;
  int  iret1, iret2;

  mydata_t data;
  data.var1 = 0;
  data.total = 0;

  iret1 = pthread_create( &thread1, NULL, thread_function, (void*) &data);
  iret2 = pthread_create( &thread2, NULL, thread_function, (void*) &data);

  pthread_join( thread1, NULL);
  pthread_join( thread2, NULL);

  return data.total;
}
