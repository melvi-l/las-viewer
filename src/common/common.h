#ifndef COMMON_H
#define COMMON_H

#define BASE_IMPLEMENTATION
#include "base.h"

#include "host/plateform.h"
#include "lib/cloud.h"
#include "lib/renderer.h"

#define max_point_count 1ULL << 16

typedef struct Application {
    Arena *scratch;
    Renderer renderer;
    Platform platform;
    Cloud cloud;
} Application;

#endif
