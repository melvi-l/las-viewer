#ifndef API_H
#define API_H

#include "common.h"

typedef struct {
  void (*load)(Application *app);
  void (*unload)(Application *app);
  void (*update)(Application *app);
} LibAPI;

#endif // API_H
