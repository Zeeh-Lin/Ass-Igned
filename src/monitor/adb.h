#ifndef __ADB_H__
#define __ADB_H__

#include "common.h"

typedef struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_t;

void init_regex();
word_t expr(char *e, bool *success);

#endif
