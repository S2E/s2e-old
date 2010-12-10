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

int main(int argc, char **argv)
{
  return fib(20);
}
