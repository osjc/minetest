#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "serstrm.h"

#define DEFAULT_BUFFER_SIZE 8192

typedef struct BinStream {
  u16 DataSize;
  u16 BufferSize;
  u8 *DataStart;
  int Handle;
  u16 LastReadSize;
  u8 Buffer[1];
} BinStream;

static void ReinitializeStream(BinStream *S)
{
  S->DataSize = 0;
  S->DataStart = S->Buffer;
  S->LastReadSize = 0;
}

void *CreateStream(u32 BufferSize)
{
  BinStream *S;

  S = malloc(BufferSize+sizeof(BinStream)-1);
  if (S!=NULL) {
    S->BufferSize = BufferSize;
    S->Handle = -1;
    ReinitializeStream(S);
  }
  return(S);
}

int CloseStream(BinStream *S)
{
  int Result;

  Result=0;
  if (S->Handle!=0) {
    Result=close(S->Handle);
  }
  free(S);
  return Result;
}

BinStream *CreateInputStream(u32 BufferSize)
{
  return CreateStream(BufferSize);
}

BinStream *CreateStandardInputStream(void)
{
  return CreateStream(DEFAULT_BUFFER_SIZE);
}

bool OpenBinStreamInput(BinStream *is, const char *FileName)
{
  is->Handle=open(FileName, O_RDONLY);
  return is->Handle!=-1;
}

static u8 *PrepareInputBlock(BinStream *is, u16 Size)
{
  u16 ConsumedAmount;
  u16 PendingAmount;
  size_t ReadSize;
  u8* ReadPos;
  ssize_t Transferred;
  u8 *Result;

  ConsumedAmount = is->DataStart - is->Buffer;
  PendingAmount = is->DataSize - ConsumedAmount;
  if (PendingAmount<Size) {
    if (is->Handle == -1) {
/*			throw new SerializationError(
				"Data fetch with no associated input stream"
			);*/
    }
    if (PendingAmount == 0) {
      is->DataStart=is->Buffer;
      is->DataSize=0;
      ReadSize=is->BufferSize-16;
    } else {
      ReadSize=Size-PendingAmount;
    }
    ReadPos=is->Buffer+is->DataSize;
    Transferred=read(is->Handle, ReadPos, ReadSize);
    if (Transferred<0) {
      return NULL;
    }
    is->DataSize+=Transferred;
    PendingAmount+=Transferred;
    if (PendingAmount<Size) {
      return NULL;
    }
  }
  Result = is->DataStart;
  is->DataStart+=Size;
  is->LastReadSize=Size;
  return Result;
}

u8 ReadU8(BinStream *is)
{
  u8 *Data;
  u8 Result;

  Data=PrepareInputBlock(is, 1);
  Result=0;
  if (Data!=NULL) {
    Result=Data[0]<<0;
  }
  return Result;
}

u16 ReadU16(BinStream *is)
{
  u8 *Data;
  u16 Result;

  Data=PrepareInputBlock(is, 2);
  Result=0;
  if (Data!=NULL) {
    Result=Data[0]<<8 | Data[1]<<0;
  }
  return Result;
}

u32 ReadU32(BinStream *is)
{
  u8 *Data;
  u32 Result;

  Data=PrepareInputBlock(is, 4);
  Result=0;
  if (Data!=NULL) {
    Result=Data[0]<<24 | Data[1]<<16 | Data[2]<<8 | Data[3]<<0;
  }
  return Result;
}

s16 ReadS16(BinStream *is)
{
  return (s16)ReadU16(is);
}

s32 ReadS32(BinStream *is)
{
  return (s32)ReadU32(is);
}

void GoBack(BinStream *is)
{
  if (is->LastReadSize==0) {
    //bug("Attempt to go back more than one time");
  }
  is->DataStart-=is->LastReadSize;
  is->LastReadSize = 0;
}

int CloseInputStream(BinStream *is)
{
  return CloseStream(is);
}
