#include <stdlib.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"

void ideal_diode_test(void *args);
void high_side_switches_test(void *args);

int main(void)
{
    System_init();
    Board_init();

    ideal_diode_test(NULL);
    // high_side_switches_test(NULL);

    Board_deinit();
    System_deinit();

    return 0;
}
