#ifndef COMMON_H
#define COMMON_H

#define BASE_IMPLEMENTATION
#include "base.h"

#include "host/plateform.h"
#include "host/renderer.h"

typedef struct Application {
    Arena *scratch;
    Renderer renderer;
    Platform platform;
} Application;

#endif
