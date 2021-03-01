
#ifndef _FONT_DIRECT_H_
#define _FONT_DIRECT_H_


#define FONTW 8
#define FONTH 8

void font_draw(uint32_t x_pos, uint32_t y_pos, uint32_t color, uint32_t scale, char *text);
void font_drawf(uint32_t *buf,uint32_t x_pos, uint32_t y_pos, uint32_t color, uint32_t scale, char *text,uint32_t height,uint32_t width);

#endif /* _FONT_DIRECT_H_ */
