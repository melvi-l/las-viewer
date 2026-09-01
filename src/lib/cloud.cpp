#include "common/common.h"

#define FACTOR INT16_MAX
static bool cd_init(Cloud *cloud) {
    srand(69);
    cloud->arena = arena_create(ARENA_DEFAULT_BLOCK_SIZE);
    cloud->point_count = max_point_count;
    printf("[CD] creating %lu\n", cloud->point_count);
    printf("[CD] alea %d\n", rand());
    cloud->points = ARENA_PUSH_ARRAY(cloud->arena, cloud->point_count, Point);
    return true;
}
static bool cd_update(Cloud *cloud) {
    (void)cloud;
    for (u64 i = 0; i < cloud->point_count; i++) {
        Point *point = &cloud->points[i];
        point->x = (i16)(2 * (rand() % FACTOR)) - (i16)FACTOR;
        point->y = (i16)(2 * (rand() % FACTOR)) - (i16)FACTOR;
        point->z = (i16)(2 * (rand() % FACTOR)) - (i16)FACTOR;
        point->rgba = ((u8)point->x << 24) |
                      ((u8)point->y << 16) |
                      ((u8)point->z << 8) |
                      255;
    }
    return true;
}
// static bool cd_destroy(Cloud *cloud) {
//     (void)cloud;
//     return true;
// }
