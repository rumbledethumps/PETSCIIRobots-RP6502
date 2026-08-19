#ifndef PETSCII_PSG_H
#define PETSCII_PSG_H

void psg_init(void);
void __fastcall__ plat_psg_note(unsigned char note);
void __fastcall__ plat_psg_effect(unsigned char note);

#endif
