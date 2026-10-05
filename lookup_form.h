
#include <stdint.h>

extern uint32_t five_step_ranks[];
extern uint8_t five_step_distance[];

#define FIVE_STEP_NUM 12224
#define get_five_step_distance(i) ((five_step_distance[i >> 1] >> ((i & 1) << 2)) & 0xF);
