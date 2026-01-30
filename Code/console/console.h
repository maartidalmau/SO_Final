#ifndef SOF1_CONSOLE_H
#define SOF1_CONSOLE_H

//Standart libraries
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//Project libraries
#include "utils.h"
#include "dataStructures.h"
#include "list.h"
#include "trade.h"
#include "client.h"
#include "frame.h"
#include "allianceHandler.h"

void consoleLogic(Maester *maester);

#endif // SOF1_CONSOLE_H
