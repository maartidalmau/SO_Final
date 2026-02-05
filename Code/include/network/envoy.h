#ifndef ENVOY_H
#define ENVOY_H

#include <sys/wait.h>

#include "dataStructures.h"

void initEnvoys(Maester *maester);
void destroyEnvoys(Maester *maester);

#endif