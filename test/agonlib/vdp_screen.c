/* <agon/vdp/screen.h> against libagon: see capture.h. */
#include "capture.h"
#include <agon/vdp.h>

int main(void)
{
    CALL(vdp_clear_screen());
    CALL(vdp_cursor_tab(5, 7));
    CALL(vdp_cursor_enable(true));
    CALL(vdp_cursor_home());
    done();
    return 0;
}
