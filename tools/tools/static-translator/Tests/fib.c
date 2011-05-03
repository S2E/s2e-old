int main(int argc, char **argv)
{
  int a = 1, b = 1, i, n=20;
    for (i = 3; i <= n; i++) {
        int c = a + b;
        a = b;
        b = c;
    }           
//    printf("%d\n", b);
    return b;
}
