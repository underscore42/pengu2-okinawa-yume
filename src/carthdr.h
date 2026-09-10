#ifndef CARTHDR_H
#define CARTHDR_H
#include "ngpc.h"
typedef void (*FuncPtr)(void);
#ifdef CARTHDR_IMPL
extern void main(void);
const char Licensed[28]  = " LICENSED BY SNK CORPORATION";
const FuncPtr ptr        = main;
const short CartID       = 0x0056;
const short System       = 0x1000;      /* colour mode - byte at 0x23 */
const char CartTitle[12] = "PENGU 2     ";
const long Reserved[4]   = {0,0,0,0};
#endif
#endif
