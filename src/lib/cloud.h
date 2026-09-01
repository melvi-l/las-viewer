#include "common/base.h"

typedef struct Point { // 10
    i16 x, y, z;       // 2 * 3 = 6
    u32 rgba;          // 4
} Point;

typedef struct Cloud {
    Arena *arena;
    u64 point_count;
    Point *points;
} Cloud;
