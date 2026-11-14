#ifndef BF006_SNAPSHOT4G_H
#define BF006_SNAPSHOT4G_H

#include "system.h"

#if (Model_OUT == Model_1C1)
void Snapshot4G_Process(void); //20261114-2 add 主循环的只读快照入口
void Snapshot4G_Before_Save(void);
#endif

#endif
