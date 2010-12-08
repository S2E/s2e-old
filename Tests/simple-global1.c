int g_notInited;
int g_inited = 2000;

int main(int argc, char **argv)
{
  g_notInited = 5000;
  return g_inited + g_notInited;
}
