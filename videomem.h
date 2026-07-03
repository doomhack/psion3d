#ifndef VIDEOMEM_H
#define VIDEOMEM_H

#include "fp_types.h"

void pokeVideoMem(void);
void blitVideoMem(u8* black, u8* grey);
void disableMemProtection(void);
void enableMemProtection(void);

#endif
