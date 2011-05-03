int fib(int n)
{
int a = 1, b = 1, i;
    for (i = 3; i <= n; i++) {
        int c = a + b;
        a = b;
        b = c;
    }
return b;
}

int dummyfunc1(int n)
{
  return 42;
}

typedef int (*funcptr_t)(int);

int main(int argc, char **argv)
{
funcptr_t g_funcs[] = {
  fib, dummyfunc1
};


  return g_funcs[argc-1](20);
}
