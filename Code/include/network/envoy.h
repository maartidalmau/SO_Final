#ifndef ENVOY_H
#define ENVOY_H

#include <sys/wait.h>
#include <stdio.h>

#include "dataStructures.h"

void createEnvoys(Maester *maester, volatile sig_atomic_t *envoyRunning);

#endif