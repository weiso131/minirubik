
#include <stdint.h>

extern uint32_t five_step_ranks[];
extern uint8_t five_step_first_move[];

#define FIVE_STEP_NUM 12224
#define get_five_step_first_move(i) ((five_step_first_move[(i) >> 1] >> (((i) & 1) << 2)) & 0xF)

extern uint32_t eleven_step_ranks[];
extern uint8_t eleven_step_first_move[];

#define ELEVEN_STEP_NUM 2644
#define get_eleven_step_first_move(i) ((eleven_step_first_move[(i) >> 1] >> (((i) & 1) << 2)) & 0xF)
