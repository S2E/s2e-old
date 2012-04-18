/**
 * Quick sort concolic execution demo.
 * Code taken from http://en.wikibooks.org/wiki/Algorithm_Implementation/Sorting/Quicksort#C
 */

#include <s2e.h>

static void swap(void *x, void *y, size_t l)
{
   char *a = x, *b = y, c;
   while(l--) {
      c = *a;
      *a++ = *b;
      *b++ = c;
   }
}

static void sort(char *array, size_t size, int (*cmp)(void*,void*), int begin, int end)
{
   if (end > begin) {
      void *pivot = array + begin;
      int l = begin + size;
      int r = end;
      while(l < r) {
         if (cmp(array+l,pivot) <= 0) {
            l += size;
         } else {
            r -= size;
            swap(array+l, array+r, size);
         }
      }
      l -= size;
      swap(array+begin, array+l, size);
      sort(array, size, cmp, begin, l);
      sort(array, size, cmp, r, end);
   }
}

void qsort(void *array, size_t nitems, size_t size, int (*cmp)(void*,void*))
{
   sort(array, size, cmp, 0, (nitems-1)*size);
}

typedef int type;

int type_cmp(void *a, void *b)
{
    return (*(type*)a)-(*(type*)b);
}

/*
procedure bubbleSort( A : list of sortable items )
  repeat
    swapped = false
    for i = 1 to length(A) - 1 inclusive do:
      if A[i-1] > A[i] then
        swap( A[i-1], A[i] )
        swapped = true
      end if
    end for
  until not swapped
end procedure
*/

void bubble_sort(char *array, unsigned size)
{
    int swapped;
    unsigned i;
    do {
        swapped = 0;
        for (i = 1; i<size; ++i) {
            if (array[i-1] > array[i]) {
                unsigned tmp = array[i-1];
                array[i-1] = array[i];
                array[i] = tmp;
                swapped = 1;
            }
        }
    } while (swapped);
}

int main(void)
{
  int num_list[]={5,4,3,2,1};

  s2e_enable_forking();
  s2e_make_concolic(&num_list, sizeof(num_list), "array");

  int len=sizeof(num_list)/sizeof(type);
  char *sep="";
  int i;
  qsort(num_list,len,sizeof(type),type_cmp);

  printf("sorted_num_list={");
  for(i=0; i<len; i++){
    printf("%s%d",sep,num_list[i]);
    sep=", ";
  }
  printf("};\n");

  s2e_kill_state(0, "Sort completed");

  return 0;
}
