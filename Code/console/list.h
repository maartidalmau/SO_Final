#ifndef SOF1_LIST_H
#define SOF1_LIST_H

//Standard libraries
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//Project libraries
#include "utils.h"
#include "dataStructures.h"

void listRealms(Maester *maester);
void listInventory(Maester *maester);

#endif