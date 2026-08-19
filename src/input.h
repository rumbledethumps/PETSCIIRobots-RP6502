#ifndef PETSCII_INPUT_H
#define PETSCII_INPUT_H

void input_init(void);
void plat_input_poll(void);   /* the keyboard scan, from the VSYNC interrupt */
unsigned char plat_getin(void);         /* what KERNAL GETIN returned */
void plat_clear_key_buffer(void);

#endif
