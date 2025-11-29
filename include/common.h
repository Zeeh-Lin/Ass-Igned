#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

#include "macro.h"
#include "log.h"

typedef uint32_t word_t;
typedef int32_t  sword_t;

#define FMT_WORD "0x%08" PRIx32
#define FMT_BYTE "0x%02" PRIx32

#define FMT_HEX  "%" PRIx32
#define FMT_INT  "%" PRId32
#define FMT_UINT "%" PRIu32

enum { ASS_RUNNING, ASS_STOP, ASS_END, ASS_ABORT, ASS_QUIT };

typedef struct {
  int state;
  uint32_t halt_pc;
  uint32_t halt_ret;
} ass_state_t;

extern ass_state_t ass_state;

#endif