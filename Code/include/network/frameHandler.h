#ifndef FRAMEHANDLER_H
#define FRAMEHANDLER_H

#define _GNU_SOURCE


#include "frame.h"
#include "router.h"
#include "dataStructures.h"
#include "allianceHandler.h"
#include "envoy.h"
#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>


void processFrame(Maester *maester, Frame *frame, int fromSocket);

void handlePingPong(Maester *maester, Frame *frame, int fromSocket);

void handleDisconnect(Maester *maester, Frame *frame);

void handleAllianceRequest(Maester *maester, Frame *frame, int fromSocket);

void handleAllianceResponse(Maester *maester, Frame *frame, int fromSocket);

void handleProductListRequest(Maester *maester, Frame *frame, int fromSocket);

void handleTradeRequest(Maester *maester, Frame *frame, int fromSocket);

void handleOrderResponse(Maester *maester, Frame *frame, int fromSocket);

#endif // FRAMEHANDLER_H
