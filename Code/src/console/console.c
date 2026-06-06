#include "console.h"

int commandHandler(char **command, Maester *maester) {
    if (!command || !*command) return 0;
    
    int exit = 0;
    char *tokens[MAX_TOKENS];
    
    int count = parseCommand(*command, tokens);

    if (count == 0) {
        customWrite(1, RED "Unknown command\n" RESET);
        safeFree((void**)command);
        return 0;
    }

    // LIST REALMS
    if (strcasecmp(tokens[0], "LIST") == 0 && count >= 2) {
        if (strcasecmp(tokens[1], "REALMS") == 0) {
            if (count == 2) {
                listRealms(maester);
            } else {
                customWrite(1, YELLOW "Did you mean LIST REALMS? Please review syntax.\n" RESET);
            }
        }
        // LIST PRODUCTS [realm]
        else if (strcasecmp(tokens[1], "PRODUCTS") == 0) {
            if (count == 2) {
                listInventory(maester);
            } else if (count == 3) {
                if (!hasAlliance(maester, tokens[2])) {
                    customWrite(1, RED "ERROR | No alliance with this realm. Forge an alliance first.\n" RESET);
                } else {
                    sendProductListRequest(maester, tokens[2]);
                }
            } else {
                customWrite(1, YELLOW "Did you mean LIST PRODUCTS [realm]? Please review syntax.\n" RESET);
                customWrite(1, CYAN "Usage: LIST PRODUCTS [realm]\n" RESET);
            }
        } else {
            customWrite(1, RED "Unknown command\n" RESET);
        }
    }
    // PLEDGE
    else if (strcasecmp(tokens[0], "PLEDGE") == 0) {
        if (count == 1) {
            customWrite(1, YELLOW "Did you mean to send a pledge? Please review syntax.\n" RESET);
            customWrite(1, CYAN "Usage: PLEDGE <realm> <sigil.png>\n" RESET);
        } 
        else if (strcasecmp(tokens[1], "STATUS") == 0 && count == 2) {
            customWrite(1, MAGENTA "--- Alliance Status ---\n" RESET);
            
            // Protegir accés a alliances amb mutex
            pthread_mutex_lock(&maester->alliances_mutex);
            
            if (maester->numAlliances == 0) {
                customWrite(1, YELLOW "No alliances established.\n" RESET);
            } else {
                for (int i = 0; i < maester->numAlliances; i++) {
                    char *msg;
                    const char *status_str;
                    switch (maester->alliances[i].status) {
                        case ALLIANCE_PENDING: status_str = "PENDING (waiting response)"; break;
                        case ALLIANCE_ACTIVE: status_str = "ALLIED"; break;
                        case ALLIANCE_FAILED: status_str = "FAILED"; break;
                        default: status_str = "NONE"; break;
                    }
                    asprintf(&msg, "  - %s: %s\n", maester->alliances[i].name, status_str);
                    customWrite(1, msg);
                    free(msg);
                }
            }
            
            pthread_mutex_unlock(&maester->alliances_mutex);
        } 
        else if (strcasecmp(tokens[1], "RESPOND") == 0) {
            if (count < 4) {
                customWrite(1, YELLOW "Did you mean to respond to a pledge? Please review syntax.\n" RESET);
                customWrite(1, CYAN "Usage: PLEDGE RESPOND <realm> <ACCEPT|REJECT>\n" RESET);
            } else if (count == 4 && (strcasecmp(tokens[3], "ACCEPT") == 0 || strcasecmp(tokens[3], "REJECT") == 0)) {
                int accept = (strcasecmp(tokens[3], "ACCEPT") == 0) ? PLEDGE_ACCEPT : PLEDGE_REJECT;
                sendAllianceResponse(maester, tokens[2], accept);
            } else {
                customWrite(1, RED "Unknown command\n" RESET);
                customWrite(1, CYAN "Usage: PLEDGE RESPOND <realm> <ACCEPT|REJECT>\n" RESET);
            }
        } 
        else if (count == 3) {
            // PLEDGE <realm> <sigil> - Enviar petició d'aliança
            sendAllianceRequest(maester, tokens[1], tokens[2]);
        } 
        else {
            customWrite(1, RED "Unknown command\n" RESET);
            customWrite(1, CYAN "Usage: PLEDGE <realm> <sigil.png>\n" RESET);
        }
    }
    // START TRADE
    else if (strcasecmp(tokens[0], "START") == 0) {
        if (count == 1) {
            customWrite(1, YELLOW "Did you mean to start trade? Please review syntax.\n" RESET);
            customWrite(1, CYAN "Usage: START TRADE <realm>\n" RESET);
        } 
        else if (strcasecmp(tokens[1], "TRADE") == 0) {
            if (count == 3) {
                Trade *trade = malloc(sizeof(Trade));
                if (!trade) {
                    customWrite(1, RED "Memory allocation error.\n" RESET);
                    safeFree((void**)command);
                    return 0;
                }
                
                trade->kingdom = strdup(tokens[2]);
                trade->products = NULL;
                trade->numProducts = 0;
                trade->totalAmount = 0;

                safeFree((void**)command);
                startTrade(trade, maester);
                freeTrade(&trade);
                return 0;
            } else {
                customWrite(1, YELLOW "Did you mean START TRADE <realm>? Please review syntax.\n" RESET);
                customWrite(1, CYAN "Usage: START TRADE <realm>\n" RESET);
            }
        } 
        else {
            customWrite(1, RED "Unknown command\n" RESET);
        }
    }
    // ENVOY STATUS
    else if (strcasecmp(tokens[0], "ENVOY") == 0) {
        if (count == 1) {
            customWrite(1, YELLOW "Did you mean ENVOY STATUS? Please review syntax.\n" RESET);
            customWrite(1, CYAN "Usage: ENVOY STATUS\n" RESET);
        } 
        else if (strcasecmp(tokens[1], "STATUS") == 0 && count == 2) {
            customWrite(1, MAGENTA "--- Envoy Status ---\n" RESET);
            pthread_mutex_lock(&maester->workersInfo->workers_mutex);
            for (int i = 0; i < maester->envoys; i++) {
                char *msg;
                const char *status = "UNKNOWN";
                if (maester->envoysAvailable != NULL) {
                    status = maester->envoysAvailable[i] ? "FREE" : "BUSY";
                }
                if (!maester->envoysAvailable[i] && maester->envoyMissions != NULL && maester->envoyMissions[i] != NULL) {
                    asprintf(&msg, "  - Envoy %d: %s (%s)\n", i + 1, status, maester->envoyMissions[i]);
                } else {
                    asprintf(&msg, "  - Envoy %d: %s\n", i + 1, status);
                }
                customWrite(1, msg);
                free(msg);
            }
            pthread_mutex_unlock(&maester->workersInfo->workers_mutex);
        } 
        else {
            customWrite(1, RED "Unknown command\n" RESET);
            customWrite(1, CYAN "Usage: ENVOY STATUS\n" RESET);
        }
    }
    // EXIT
    else if (strcasecmp(tokens[0], "EXIT") == 0) {
        if (count == 1) {
            exit = 1;
        } else {
            customWrite(1, YELLOW "Did you mean EXIT? Please review syntax.\n" RESET);
            customWrite(1, CYAN "Usage: EXIT\n" RESET);
        }
    }
    // HELP
    else if (strcasecmp(tokens[0], "HELP") == 0) {
        customWrite(1, MAGENTA "\n=== Available Commands ===\n" RESET);
        customWrite(1, CYAN "LIST REALMS" RESET " - List all known realms\n");
        customWrite(1, CYAN "LIST PRODUCTS [realm]" RESET " - List products in inventory\n");
        customWrite(1, CYAN "PLEDGE <realm> <sigil.png>" RESET " - Send a pledge to a realm\n");
        customWrite(1, CYAN "PLEDGE STATUS" RESET " - Check alliance status\n");
        customWrite(1, CYAN "PLEDGE RESPOND <realm> <ACCEPT|REJECT>" RESET " - Respond to a pledge\n");
        customWrite(1, CYAN "START TRADE <realm>" RESET " - Start trade session with realm\n");
        customWrite(1, CYAN "ENVOY STATUS" RESET " - Check envoy status\n");
        customWrite(1, CYAN "EXIT" RESET " - Exit the program\n");
        customWrite(1, MAGENTA "==========================\n\n" RESET);
    }
    // Unknown command
    else {
        customWrite(1, RED "Unknown command\n" RESET);
        customWrite(1, CYAN "Type HELP for available commands\n" RESET);
    }

    safeFree((void**)command);
    return exit;
}

void consoleLogic(Maester *maester) {
    char *command = NULL, *msg = NULL;
    int exitStatus = 0;

    asprintf(&msg, "%sMaester of %s initialized. The board is set.%s\n", GREEN, maester->name, RESET);
    customWrite(1, msg);
    free(msg);

    while (!exitStatus && maester->running) {
        
        customWrite(1, GREEN "$ " RESET);
        customRead(0, &command, '\n');
        
        if (!command) continue;
        
        exitStatus = commandHandler(&command, maester);
    }

    //Ha sigut Ctrl+C
    if (!maester->running) {
        customWrite(1, YELLOW "\nShutting down Maester services... (Ctrl + C detected)\n" RESET);
    }

    //L'usuari ha fet exit
    if (exitStatus) {
        maester->running = 0;
    }
}
