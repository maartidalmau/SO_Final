#ifndef SOF1_TRADE_H
#define SOF1_TRADE_H

//Standard libraries
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//Project libraries
#include "utils.h"
#include "dataStructures.h"
#include "envoy.h"
#include "client.h"
#include "router.h"
#include "ipc.h"

void startTrade(Trade *trade, Maester *maester);

#endif