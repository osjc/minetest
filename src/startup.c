#include <stdio.h>

int MainCPP(int argc, char *argv[]);

int main(int argc, char *argv[])
{
  int Result;

  Result=MainCPP(argc, argv);

  if (Result==-1) {
    fprintf(stderr, "The program was terminated via an uncaught exception");
    Result=1;
  }

  return(Result);
}
