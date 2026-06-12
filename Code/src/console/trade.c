#include "trade.h"

static RemoteCatalog *getTradeCatalog(Trade *trade, Maester *maester) {
    if (!trade || !maester) {
        return NULL;
    }

    return findRemoteCatalog(maester, trade->kingdom);
}

static int isProductInRemoteCatalog(char *productName, RemoteCatalog *catalog) {
    if (!productName || !catalog) {
        return 0;
    }

    for (int i = 0; i < catalog->numProducts; i++) {
        if (strcasecmp(catalog->products[i], productName) == 0) {
            return 1;
        }
    }

    return 0;
}

int checkRealm(Trade *trade, Maester *maester) {
    if (!trade || !maester) {
        return 0;
    }

    // Vàlid si el regne és a la taula de rutes...
    for (int i = 0; i < maester->numRoutes; i++) {
        if (strcasecmp(trade->kingdom, maester->routes[i].name) == 0) {
            return 1;
        }
    }

    // ...o si ja hi tenim una aliança (s'hi pot arribar encara que no estigui
    // llistat a les rutes estàtiques: per hops o per la IP directa compartida).
    int status = ALLIANCE_NONE;
    if (getAllianceInfo(maester, trade->kingdom, NULL, NULL, &status, NULL)) {
        return 1;
    }

    return 0;
}

int findTradeProduct(Trade *trade, char *productName) {
    if (!trade || !productName) return -1;
    
    for (int i = 0; i < trade->numProducts; i++) {
        if (strcasecmp(trade->products[i].name, productName) == 0) {
            return i;
        }
    }
    return -1;
}

int addProductToTrade(Trade *trade, char *productName, int quantity) {
    if (!trade || !productName || quantity <= 0) {
        return trade ? trade->numProducts : 0;
    }
    
    int idx = findTradeProduct(trade, productName);

    if (idx != -1) {
        // Producte ja existeix, augmentar quantitat
        trade->products[idx].amount += quantity;
    } else {
        // Nou producte
        TradeProduct *tmp = realloc(
            trade->products,
            (trade->numProducts + 1) * sizeof(TradeProduct)
        );

        if (!tmp) {
            customWrite(1, RED "Memory allocation error.\n" RESET);
            return trade->numProducts;
        }

        trade->products = tmp;
        trade->products[trade->numProducts].name = strdup(productName);
        trade->products[trade->numProducts].amount = quantity;
        trade->numProducts++;
    }

    trade->totalAmount += quantity;
    
    char *msg;
    asprintf(&msg, GREEN "Added %d x %s\n" RESET, quantity, productName);
    customWrite(1, msg);
    free(msg);
    
    return trade->numProducts;
}

int removeProductFromTrade(Trade *trade, char *productName, int quantity) {
    if (!trade || !productName || quantity <= 0) {
        return trade ? trade->numProducts : 0;
    }
    
    int i = findTradeProduct(trade, productName);

    if (i == -1) {
        customWrite(1, RED "Product not in trade list.\n" RESET);
        return trade->numProducts;
    }

    if (trade->products[i].amount < quantity) {
        customWrite(1, RED "Cannot remove more than listed.\n" RESET);
        return trade->numProducts;
    }

    trade->products[i].amount -= quantity;
    trade->totalAmount -= quantity;

    char *msg;
    asprintf(&msg, GREEN "Removed %d x %s\n" RESET, quantity, productName);
    customWrite(1, msg);
    free(msg);

    // Si la quantitat arriba a 0, eliminar el producte
    if (trade->products[i].amount == 0) {
        free(trade->products[i].name);

        for (int j = i; j < trade->numProducts - 1; j++) {
            trade->products[j] = trade->products[j + 1];
        }

        trade->numProducts--;

        if (trade->numProducts > 0) {
            TradeProduct *tmp = realloc(
                trade->products,
                trade->numProducts * sizeof(TradeProduct)
            );
            if (tmp) {
                trade->products = tmp;
            }
        } else {
            free(trade->products);
            trade->products = NULL;
        }
    }

    return trade->numProducts;
}

char* generateTradeFileName(Trade *trade) {
    if (!trade || !trade->kingdom) return NULL;
    
    char *fileName;
    asprintf(&fileName, "trade_%s.txt", trade->kingdom);
    return fileName;
}

int writeTradeFile(Trade *trade, char *fileName) {
    if (!trade || !fileName) return 0;
    
    int fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        customWrite(1, RED "Error creating trade file.\n" RESET);
        return 0;
    }

    for (int i = 0; i < trade->numProducts; i++) {
        char *msg;
        asprintf(&msg, "%d x %s\n", 
                trade->products[i].amount, 
                trade->products[i].name);
        customWrite(fd, msg);
        free(msg);
    }

    close(fd);
    return 1;
}

int handleSendCommand(Trade *trade, Maester *maester) {
    if (!trade || !maester) return 0;

    if (trade->totalAmount == 0) {
        customWrite(1, RED "No products listed for trade.\n" RESET);
        return 0;
    }

    char *fileName = generateTradeFileName(trade);
    if (!fileName) {
        customWrite(1, RED "Memory allocation error.\n" RESET);
        return 0;
    }

    int success = 0;
    if (writeTradeFile(trade, fileName)) {
        int envoyIndex = reserveEnvoy(maester);
        if (envoyIndex < 0) {
            customWrite(1, RED "ERROR | No envoys available\n" RESET);
            free(fileName);
            return 0;
        }

        // Si som aliats, anem directes a la seva IP; si no, per la taula de rutes.
        char *targetIp = NULL;
        int targetPort = 0;
        char *allyIp = NULL;
        int allyPort = 0;
        int allyStatus = ALLIANCE_NONE;
        if (getAllianceInfo(maester, trade->kingdom, &allyIp, &allyPort, &allyStatus, NULL) &&
            allyStatus == ALLIANCE_ACTIVE && allyIp && allyPort > 0) {
            targetIp = allyIp;        // prenem la propietat del punter
            targetPort = allyPort;
        } else {
            if (allyIp) free(allyIp);
            if (!getRouteInfo(maester, trade->kingdom, &targetIp, &targetPort)) {
                if (!getRouteInfo(maester, NULL, &targetIp, &targetPort)) {
                    releaseEnvoy(maester, envoyIndex);
                    char *msg;
                    asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, trade->kingdom);
                    customWrite(1, msg);
                    free(msg);
                    free(fileName);
                    return 0;
                }
            }
        }

        IpcRequest request;
        IpcResponse response;
        memset(&request, 0, sizeof(IpcRequest));

        request.type = IPC_SEND_TRADE_FILE;
        strncpy(request.source_realm, maester->name, IPC_REALM_SIZE - 1);
        strncpy(request.source_ip, maester->ip, IPC_IP_SIZE - 1);
        request.source_port = (uint32_t)maester->port;
        strncpy(request.target_realm, trade->kingdom, IPC_REALM_SIZE - 1);
        strncpy(request.target_ip, targetIp, IPC_IP_SIZE - 1);
        request.target_port = (uint32_t)targetPort;
        strncpy(request.path, fileName, IPC_PATH_SIZE - 1);

        char *msg;
        asprintf(&msg, "TRADE with %s", trade->kingdom);
        setEnvoyMission(maester, envoyIndex, msg);
        free(msg);

        if (dispatchEnvoyRequest(maester, envoyIndex, &request, &response) < 0 || response.status != IPC_STATUS_OK) {
            asprintf(&msg, RED "ERROR | Failed to send trade to [%s]\n" RESET, trade->kingdom);
            customWrite(1, msg);
            free(msg);
            releaseEnvoy(maester, envoyIndex);
        } else {
            releaseEnvoy(maester, envoyIndex);
            success = 1;

            // Comanda acceptada: rebem els béns -> actualitzem el NOSTRE inventari
            // (l'aliat ja ha decrementat el seu) i el persistim a stock.db. El pes
            // per unitat el traiem del catàleg remot (obtingut amb LIST PRODUCTS).
            for (int i = 0; i < trade->numProducts; i++) {
                float w = remoteCatalogWeight(maester, trade->kingdom, trade->products[i].name);
                incrementInventory(maester, trade->products[i].name, trade->products[i].amount, w);
            }
            updateStockDB(maester->stockFile, maester);

            asprintf(&msg, GREEN ">>> Order accepted by %s. Goods received, inventory updated.\n" RESET,trade->kingdom);
            customWrite(1, msg);
            free(msg);
        }

        if (targetIp) free(targetIp);
    }

    free(fileName);
    return success;
}

int handleAddCommand(Trade *trade, char *productName, int quantity, Maester *maester) {
    if (!trade || !productName || !maester) {
        return trade ? trade->numProducts : 0;
    }
    
    if (quantity <= 0) {
        customWrite(1, RED "Invalid quantity.\n" RESET);
        return trade->numProducts;
    }

    // Només es poden afegir productes del catàleg de l'ALIAT (obtingut amb
    // LIST PRODUCTS), no del propi inventari.
    RemoteCatalog *catalog = getTradeCatalog(trade, maester);
    if (!catalog || catalog->numProducts == 0) {
        customWrite(1, RED "No products available. Use LIST PRODUCTS first.\n" RESET);
        return trade->numProducts;
    }
    if (!isProductInRemoteCatalog(productName, catalog)) {
        customWrite(1, RED "Product not in remote catalog.\n" RESET);
        return trade->numProducts;
    }

    return addProductToTrade(trade, productName, quantity);
}

int handleRemoveCommand(Trade *trade, char *productName, int quantity) {
    if (!trade || !productName) {
        return trade ? trade->numProducts : 0;
    }
    
    if (quantity <= 0) {
        customWrite(1, RED "Invalid quantity.\n" RESET);
        return trade->numProducts;
    }
    
    return removeProductFromTrade(trade, productName, quantity);
}

char* buildProductName(char *tokens[], int count) {
    if (count <= 2) return NULL;

    int len = 0;
    for (int i = 1; i < count - 1; i++) {
        len += strlen(tokens[i]) + 1;
    }

    char *productName = malloc(len);
    if (!productName) return NULL;
    
    productName[0] = '\0';

    for (int i = 1; i < count - 1; i++) {
        strcat(productName, tokens[i]);
        if (i < count - 2) {
            strcat(productName, " ");
        }
    }

    return productName;
}

static void displayTradeCatalog(Trade *trade, Maester *maester) {
    RemoteCatalog *catalog = getTradeCatalog(trade, maester);
    if (!catalog || catalog->numProducts == 0) {
        customWrite(1, YELLOW "No products available. Use LIST PRODUCTS first.\n" RESET);
        return;
    }

    customWrite(1, YELLOW "Available products: " RESET);
    for (int i = 0; i < catalog->numProducts; i++) {
        char *msg;
        asprintf(&msg, "%s%s%s", CYAN, catalog->products[i], RESET);
        customWrite(1, msg);
        free(msg);
        if (i < catalog->numProducts - 1) {
            customWrite(1, YELLOW ", " RESET);
        }
    }
    customWrite(1, ".\n");
}

void startTrade(Trade *trade, Maester *maester) {
    if (!trade || !maester) return;
    
    int exit = 0;
    char *msg, *command = NULL;
    char *tokens[MAX_TOKENS];

    // Verificar que el realm existeix
    if (!checkRealm(trade, maester)) {
        customWrite(1, RED "ERROR: The specified realm does not exist.\n" RESET);
        return;
    }

    // Només es pot comerciar amb un regne amb el qual tenim una aliança ACTIVA
    int allianceStatus = ALLIANCE_NONE;
    if (!getAllianceInfo(maester, trade->kingdom, NULL, NULL, &allianceStatus, NULL) ||
        allianceStatus != ALLIANCE_ACTIVE) {
        asprintf(&msg, RED "ERROR: You must have an alliance with %s to trade.\n" RESET, trade->kingdom);
        customWrite(1, msg);
        free(msg);
        return;
    }

    asprintf(&msg, "%sEntering trade mode with %s.%s\n", MAGENTA, trade->kingdom, RESET);
    customWrite(1, msg);
    free(msg);

    // Només mostrem el catàleg de l'ALIAT (obtingut amb LIST PRODUCTS). Si encara
    // no s'ha demanat, displayTradeCatalog avisa "Use LIST PRODUCTS first".
    displayTradeCatalog(trade, maester);

    while (!exit && maester->running) {
        customWrite(1, GREEN "(trade)> " RESET);
        customRead(0, &command, '\n');

        if (!command) continue;

        int count = parseCommand(command, tokens);

        if (count == 0) {
            safeFree((void**)&command);
            continue;
        }

        if (strcasecmp(tokens[0], "ADD") == 0 && count > 2) {
            char *productName = buildProductName(tokens, count);
            if (!productName) {
                customWrite(1, RED "Memory allocation error.\n" RESET);
                safeFree((void**)&command);
                continue;
            }
            handleAddCommand(trade, productName, atoi(tokens[count - 1]), maester);
            free(productName);
        } 
        else if (strcasecmp(tokens[0], "REMOVE") == 0 && count > 2) {
            char *productName = buildProductName(tokens, count);
            if (!productName) {
                customWrite(1, RED "Memory allocation error.\n" RESET);
                safeFree((void**)&command);
                continue;
            }
            handleRemoveCommand(trade, productName, atoi(tokens[count - 1]));
            free(productName);
        } 
        else if (strcasecmp(tokens[0], "SEND") == 0 && count == 1) {
            exit = handleSendCommand(trade, maester);
        } 
        else if (strcasecmp(tokens[0], "CANCEL") == 0 && count == 1) {
            customWrite(1, YELLOW "Trade cancelled.\n" RESET);
            exit = 1;
        } 
        else if (strcasecmp(tokens[0], "LIST") == 0 && count == 1) {
            // Mostrar llista actual de comerç
            if (trade->numProducts == 0) {
                customWrite(1, YELLOW "No products in trade list.\n" RESET);
            } else {
                customWrite(1, CYAN "--- Current Trade List ---\n" RESET);
                for (int i = 0; i < trade->numProducts; i++) {
                    asprintf(&msg, "  %d x %s\n", trade->products[i].amount, trade->products[i].name);
                    customWrite(1, msg);
                    free(msg);
                }
                asprintf(&msg, YELLOW "Total items: %d\n" RESET, trade->totalAmount);
                customWrite(1, msg);
                free(msg);
            }
        }
        else if (strcasecmp(tokens[0], "HELP") == 0) {
            customWrite(1, CYAN "Trade Commands:\n" RESET);
            customWrite(1, "  ADD <product> <quantity> - Add product to trade list\n");
            customWrite(1, "  REMOVE <product> <quantity> - Remove product from list\n");
            customWrite(1, "  LIST - Show current trade list\n");
            customWrite(1, "  SEND - Send trade list and exit\n");
            customWrite(1, "  CANCEL - Cancel trade and exit\n");
        }
        else {
            customWrite(1, RED "Unknown command\n" RESET);
            customWrite(1, CYAN "Type HELP for available commands\n" RESET);
        }
        
        safeFree((void**)&command);
    }
}
