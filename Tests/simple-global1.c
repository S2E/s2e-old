int g_notInited;
int g_inited = 2;

int main(int argc, char **argv)
{
  g_notInited = 5;
  return g_inited + g_notInited;
}
