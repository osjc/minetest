#ifndef CONFIG_HEADER
#define CONFIG_HEADER
#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "serstrm.h"

#define PLAYERNAME_SIZE 20
#define VALUEGEN_SPEC_SIZE 32
#define ADDRESS_SPEC_SIZE 64
#define FLOAT_SPEC_SIZE 16

typedef struct TConfiguration {
  /* Client stuff */
  bool DedicatedServer;
  u16 WantedFps;
  u16 MaximalFps;
  u16 ViewRangeMin;
  u16 ViewRangeMax;
  u16 ScreenWidth;
  u16 ScreenHeight;
  bool HostGame;
  char Address[ADDRESS_SPEC_SIZE];
  u16 Port;
  char Name[PLAYERNAME_SIZE];
  bool RandomInput;
  u16 DeleteUnusedSectorTimeout;

  /* Server stuff */
  bool CreativeMode;
  char HeightRandMax[VALUEGEN_SPEC_SIZE];
  char HeightRandFactor[VALUEGEN_SPEC_SIZE];
  char HeightBase[VALUEGEN_SPEC_SIZE];
  char PlantsAmount[FLOAT_SPEC_SIZE];
  char RavinesAmount[FLOAT_SPEC_SIZE];
  char ObjectDataInterval[FLOAT_SPEC_SIZE];
  char ActiveObjectRange[FLOAT_SPEC_SIZE];
  u16 MaxBlockSendsPerClient;
  u16 MaxBlockSendsTotal;
} TConfiguration;

typedef enum {
  crOk,
  crFileNotFound,
  crParseError
} TConfigReadResult;

void SetDefaultSettings(TConfiguration *C);
TConfigReadResult ReadConfigFile(TConfiguration *C, const char *Name);

#ifdef __cplusplus
}
#endif
#endif
