#include <stdio.h>
#include <string.h>

#include "config.h"

int MainCPP(int argc, char *argv[], TConfiguration *C);

static TConfigReadResult ReadConfiguration(TConfiguration *C, const char *Dir)
{
  char Spec[80];

  strcpy(Spec, Dir);
  strcat(Spec, "minerworld.conf");
  return ReadConfigFile(C, Spec);
}

int main(int argc, char *argv[])
{
  int Result;
  TConfiguration C;
  TConfigReadResult CfgResult;

  SetDefaultSettings(&C);
  if (argc >= 2) {
    CfgResult=ReadConfigFile(&C, argv[1]);
  } else {
    CfgResult=ReadConfiguration(&C, "");
    if (CfgResult==crFileNotFound) {
      CfgResult=ReadConfiguration(&C, "../");
      if (CfgResult==crFileNotFound) {
        CfgResult=ReadConfiguration(&C, "../../");
      }
    }
  }
  if (CfgResult==crParseError) {
    fprintf(stderr, "The program was terminated due to config file error");
    return(1);
  }

  Result=MainCPP(argc, argv, &C);

  if (Result==-1) {
    fprintf(stderr, "The program was terminated via an uncaught exception");
    Result=1;
  }

  return(Result);
}
