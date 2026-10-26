#ifndef UI_INPUT_H
#define UI_INPUT_H

#include <3ds.h>
#include <stdbool.h>

typedef struct {
	u32 down;     /* botones pulsados en este fotograma */
	u32 held;     /* botones mantenidos */
	u32 repeat;   /* pulsación con autorepetición (para la cruceta) */
	bool touch_down; /* el lápiz acaba de tocar la pantalla */
	bool touching;
	int tx, ty;   /* posición del lápiz */
} Input;

void input_read(Input* in);
bool input_touch_in(const Input* in, float x, float y, float w, float h);

#endif
