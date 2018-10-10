#ifndef SERSTRM_HEADER
#define SERSTRM_HEADER
#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

typedef struct BinStream BinStream;

BinStream *CreateStandardInputStream(void);
BinStream *CreateInputStream(u32 BufferSize);
bool OpenBinStreamInput(BinStream *is, const char *FileName);
u8 ReadU8(BinStream *is);
u16 ReadU16(BinStream *is);
u32 ReadU32(BinStream *is);
s16 ReadS16(BinStream *is);
s32 ReadS32(BinStream *is);
void GoBack(BinStream *is);
int CloseInputStream(BinStream *is);

#ifdef __cplusplus
}
#endif
#endif
