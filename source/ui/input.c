#include "ui/input.h"

void input_read(Input* in)
{
	static bool repeat_configured;
	if (!repeat_configured) {
		hidSetRepeatParameters(18, 6);
		repeat_configured = true;
	}
	hidScanInput();
	in->down = hidKeysDown();
	in->held = hidKeysHeld();
	in->repeat = hidKeysDownRepeat();
	in->touching = (in->held & KEY_TOUCH) != 0;
	in->touch_down = (in->down & KEY_TOUCH) != 0;
	if (in->touching) {
		touchPosition t;
		hidTouchRead(&t);
		in->tx = t.px;
		in->ty = t.py;
	}
}

bool input_touch_in(const Input* in, float x, float y, float w, float h)
{
	return in->touch_down && in->tx >= x && in->tx < x + w && in->ty >= y && in->ty < y + h;
}
