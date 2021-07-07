#include <ncurses.h>

#include "smc.h"

int gety(WINDOW *w)
{
	int y, x;
	getyx(w, y, x);
	UNUSED(x);
	return y;
}

int getx(WINDOW *w)
{
	int y, x;
	getyx(w, y, x);
	UNUSED(y);
	return x;
}
