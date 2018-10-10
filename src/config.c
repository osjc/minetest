#include <string.h>
#include <stdio.h>

#include "config.h"

void SetDefaultSettings(TConfiguration *C)
{
  /* Client stuff */
  C->DedicatedServer = false;
  C->WantedFps = 30;
  C->MaximalFps = 60;
  C->ViewRangeMin = 20;
  C->ViewRangeMax = 300;
  C->ScreenWidth = 800;
  C->ScreenHeight = 600;
  C->HostGame = false;
  C->Address[0] = '\0';
  C->Port = 30000;
  C->Name[0]='\0';
  C->RandomInput = false;
  C->DeleteUnusedSectorTimeout = 1200;

  /* Server stuff */
  C->CreativeMode = false;
  strcpy(C->HeightRandMax, "constant 70");
  strcpy(C->HeightRandFactor, "constant 0.6");
  strcpy(C->HeightBase, "linear 0 35 0");
  strcpy(C->PlantsAmount, "1");
  strcpy(C->RavinesAmount, "1");
  strcpy(C->ObjectDataInterval, "0.2");
  strcpy(C->ActiveObjectRange, "2");
  C->MaxBlockSendsPerClient = 1;
  C->MaxBlockSendsTotal = 4;
}

static char ReadCharFromLine(BinStream *is)
{
  char Result;

  Result=ReadU8(is);
  if (Result=='\n') {
    GoBack(is);
  }
  if (Result=='\0') {
    Result='\n';
  }
  return Result;
}

static bool IsWhiteSpace(u8 Ch)
{
  return (Ch==' ' || Ch=='\t' || Ch=='\r');
}

static void SkipWhiteSpace(BinStream *is)
{
  u8 Ch;

  do {
    Ch=ReadCharFromLine(is);
  } while (IsWhiteSpace(Ch));
  if (Ch!='\n') {
    GoBack(is);
  }
}

static void ReadKeyword(BinStream *is, char *Dest, u8 Count)
{
  char Ch;

  Count--;
  for (;;) {
    Ch=ReadCharFromLine(is);
    if (IsWhiteSpace(Ch) || Ch=='\n') {
      break;
    }
    if (Count>0) {
      Dest[0]=Ch;
      Dest++;
      Count--;
    }
  }
  Dest[0]='\0';
}

typedef enum {
  kwFirstKeyword,

  kwDedicatedServer,
  kwWantedFps,
  kwMaximalFps,
  kwViewRangeMin,
  kwViewRangeMax,
  kwScreenWidth,
  kwScreenHeight,
  kwHostGame,
  kwAddress,
  kwPort,
  kwName,
  kwRandomInput,
  kwDeleteUnusedSectorTimeout,
  kwCreativeMode,
  kwHeightRandMax,
  kwHeightRandFactor,
  kwHeightBase,
  kwPlantsAmount,
  kwRavinesAmount,
  kwObjectDataInverval,
  kwActiveObjectRange,
  kwMaxBlockSendsPerClient,
  kwMaxBlockSendsTotal,

  kwInvalidKeyword
} TKeyword;

static TKeyword RecognizeKeyword(const char *KeyWord)
{
  const char *ValidKeywords;
  TKeyword Result;
  bool Match;
  const char *ScanPtr;
  char Ch;
  char Ch1;

  ValidKeywords = (
    "DedicatedServer"  "\xFE"
    "WantedFps" "\xFE"
    "MaximalFps" "\xFE"
    "ViewRangeMin" "\xFE"
    "ViewRangeMax" "\xFE"
    "ScreenWidth" "\xFE"
    "ScreenHeight" "\xFE"
    "HostGame" "\xFE"
    "Address" "\xFE"
    "Port" "\xFE"
    "Name" "\xFE"
    "RandomInput" "\xFE"
    "DeleteUnusedSectorTimeout" "\xFE"
    "CreativeMode" "\xFE"
    "HeightRandMax" "\xFE"
    "HeightRandFactor" "\xFE"
    "HeightBase" "\xFE"
    "PlantsAmount" "\xFE"
    "RavinesAmount" "\xFE"
    "ObjectDataInverval" "\xFE"
    "ActiveObjectRange" "\xFE"
    "MaxBlockSendsPerClient" "\xFE"
    "MaxBlockSendsTotal"
  );
  Result=kwFirstKeyword;
  Ch='\xFE';
  Match=false;
  for (;;) {
    if (Ch=='\0') {
      Result=kwInvalidKeyword;
      break;
    } else if (Ch=='\xFE') {
      if (Match) {
        break;
      }
      Result++;
      Match=true;
      ScanPtr=KeyWord;
    } else if (Match) {
      Ch1=ScanPtr[0];
      ScanPtr++;
      Match=(Ch==Ch1);
    }
    Ch=ValidKeywords[0];
    ValidKeywords++;
  }
  return(Result);
}

static const char *ParseBool(BinStream *is, bool *Result)
{
  char BoolText[16];
  char *Ptr;
  char Ch;

  ReadKeyword(is, BoolText, 16);
  Ptr=BoolText;
  for (;;) {
    Ch=Ptr[0];
    if (Ch=='\0') {
      break;
    }
    if (Ch>='A' && Ch<='Z') {
      Ch-=32;
    }
    Ptr[0]=Ch;
    Ptr++;
  }
  if (strcmp(BoolText, "yes")==0 || strcmp(BoolText, "true")==0) {
    Result[0]=true;
    return NULL;
  }
  if (strcmp(BoolText, "no")==0 || strcmp(BoolText, "false")==0) {
    Result[0]=false;
    return NULL;
  }
  return "Unrecognized boolean value";
}

static const char *ParseU16(BinStream *is, u16 *Result)
{
  char Ch;
  u16 Value;

  Value=0;
  for (;;) {
    Ch=ReadCharFromLine(is);
    if (Ch>='0' && Ch<='9') {
      if ((Value>6553) || (Value==6553 && Ch>='6')) {
        return "Value is too large for the configuration item";
      }
      Value*=10;
      Value+=Ch-'0';
    } else if (IsWhiteSpace(Ch) || Ch=='\n') {
      break;
    } else {
      return "Invalid digit in a numeric value";
    }
  }
  Result[0]=Value;
  return NULL;
}

static const char *ParseString(BinStream *is, char *Result, u16 Size)
{
  char Ch;
  bool WasWhiteSpace;
  int i;

  Size--;
  WasWhiteSpace=true;
  for (;;) {
    Ch=ReadCharFromLine(is);
    if (Ch=='\n') {
      break;
    }
    if (IsWhiteSpace(Ch)) {
      WasWhiteSpace=true;
      continue;
    } else if (WasWhiteSpace) {
      WasWhiteSpace=false;
    }
    for (i=0;i<2;i++) {
      if (i==0 && !WasWhiteSpace) {
        WasWhiteSpace=false;
        continue;
      }
      if (Size==0) {
        return "String value is too long";
      }
      if (i==0) {
        Result[0]=' ';
      } else {
        Result[0]=Ch;
      }
      Result++;
      Size--;
    }
  }
  Result[0]='\0';
  return NULL;
}

static const char *ParseValueGen(BinStream *is, char *Result)
{
  return ParseString(is, Result, VALUEGEN_SPEC_SIZE);
}

static const char *ParseConfigDataLine(TConfiguration *C, BinStream *is)
{
  char KeywordText[40];
  TKeyword Keyword;
  const char *Result;
  char Ch;

  SkipWhiteSpace(is);
  Ch=ReadCharFromLine(is);
  if (Ch=='#') {
    while (Ch!='\n') {
      Ch=ReadCharFromLine(is);
    }
  }
  if (Ch=='\n') {
    return NULL;
  }
  GoBack(is);
  ReadKeyword(is, KeywordText, 40);
  Keyword=RecognizeKeyword(KeywordText);
  SkipWhiteSpace(is);
  Ch=ReadCharFromLine(is);
  if (Ch!='=') {
    return "Expected equal sign after configuration item name";
  }
  SkipWhiteSpace(is);
  if (Keyword==kwDedicatedServer) {
    Result=ParseBool(is, &C->DedicatedServer);
  } else if (Keyword==kwWantedFps) {
    Result=ParseU16(is, &C->WantedFps);
  } else if (Keyword==kwMaximalFps) {
    Result=ParseU16(is, &C->MaximalFps);
  } else if (Keyword==kwViewRangeMin) {
    Result=ParseU16(is, &C->ViewRangeMin);
  } else if (Keyword==kwViewRangeMax) {
    Result=ParseU16(is, &C->ViewRangeMax);
  } else if (Keyword==kwScreenWidth) {
    Result=ParseU16(is, &C->ScreenWidth);
  } else if (Keyword==kwScreenHeight) {
    Result=ParseU16(is, &C->ScreenHeight);
  } else if (Keyword==kwHostGame) {
    Result=ParseBool(is, &C->HostGame);
  } else if (Keyword==kwAddress) {
    Result=ParseString(is, C->Address, ADDRESS_SPEC_SIZE);
  } else if (Keyword==kwPort) {
    Result=ParseU16(is, &C->Port);
  } else if (Keyword==kwName) {
    Result=ParseString(is, C->Name, PLAYERNAME_SIZE);
  } else if (Keyword==kwRandomInput) {
    Result=ParseBool(is, &C->RandomInput);
  } else if (Keyword==kwDeleteUnusedSectorTimeout) {
    Result=ParseU16(is, &C->DeleteUnusedSectorTimeout);
  } else if (Keyword==kwCreativeMode) {
    Result=ParseBool(is, &C->CreativeMode);
  } else if (Keyword==kwHeightRandMax) {
    Result=ParseValueGen(is, C->HeightRandMax);
  } else if (Keyword==kwHeightRandFactor) {
    Result=ParseValueGen(is, C->HeightRandFactor);
  } else if (Keyword==kwHeightBase) {
    Result=ParseValueGen(is, C->HeightBase);
  } else if (Keyword==kwPlantsAmount) {
    Result=ParseString(is, C->PlantsAmount, FLOAT_SPEC_SIZE);
  } else if (Keyword==kwRavinesAmount) {
    Result=ParseString(is, C->RavinesAmount, FLOAT_SPEC_SIZE);
  } else if (Keyword==kwObjectDataInverval) {
    Result=ParseString(is, C->ObjectDataInterval, FLOAT_SPEC_SIZE);
  } else if (Keyword==kwActiveObjectRange) {
    Result=ParseString(is, C->ActiveObjectRange, FLOAT_SPEC_SIZE);
  } else if (Keyword==kwMaxBlockSendsPerClient) {
    Result=ParseU16(is, &C->MaxBlockSendsPerClient);
  } else if (Keyword==kwMaxBlockSendsTotal) {
    Result=ParseU16(is, &C->MaxBlockSendsTotal);
  } else {
    Result="Unrecognized keyword";
  }
  return Result;
}

static void Error(bool *IsFirstError, u16 LineNumber, const char *Msg)
{
  if (IsFirstError[0]) {
    printf("Errors in configuration file:\n");
    IsFirstError[0]=false;
  }
  printf("%5d: %s\n", LineNumber, Msg);
}

static bool ReadConfigData(TConfiguration *C, BinStream *is)
{
  u16 LineNumber;
  const char *Result;
  bool IsFirstError;
  u8 Ch;
  bool GarbageReported;

  LineNumber=0;
  IsFirstError=true;
  for (;;) {
    LineNumber+=1;
    Result=ParseConfigDataLine(C, is);
    if (Result!=NULL) {
      Error(&IsFirstError, LineNumber, Result);
    }
    GarbageReported=(Result!=NULL);
    for (;;) {
      Ch=ReadU8(is);
      if (Ch=='\n' || Ch=='\0') {
        break;
      }
      if (GarbageReported || IsWhiteSpace(Ch)) {
        continue;
      }
      if (!GarbageReported) {
        Error(&IsFirstError, LineNumber, "Garbage at end of line");
        GarbageReported=true;
      }
    }
    if (Ch=='\0') {
      break;
    }
  }
  return IsFirstError;
}

TConfigReadResult ReadConfigFile(TConfiguration *C, const char *Name)
{
  BinStream *is;
  bool Result;

  is=CreateStandardInputStream();
  Result=OpenBinStreamInput(is, Name);
  if (!Result) {
    return crFileNotFound;
  }
  Result=ReadConfigData(C, is);
  if (!Result) {
    return crParseError;
  }
  CloseInputStream(is);
  return(crOk);
}
