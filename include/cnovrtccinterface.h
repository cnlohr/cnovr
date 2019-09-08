#ifndef _CNOVRTCCINTERFACE_H
#define _CNOVRTCCINTERFACE_H

//User apps should not include this file.

struct TCCState;
typedef struct TCCState TCCState;

void InternalShutdownTCC( TCCState * tcc );
void InternalPopulateTCC( TCCState * tcc );

#endif

