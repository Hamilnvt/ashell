#include <stdio.h>


int main(void)
{

    printn_fg_text_begin("Green foreground", GREEN);
    printn_bg_text_begin("Blue background", BLUE);
    printn_color_text("Blue background and green foreground", BLUE, GREEN);
    return 0;
}
