#include "commands.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


void consoleLogic(Maester *maester) {
    char *command = NULL;
    int exitStatus = 0;

    while (!exitStatus && maester->running) {
        customWrite(1, GREEN "$ " RESET);
        customRead(0, &command, '\n');
        
        if (!command) continue;
        
        exitStatus = commandHandler(&command, maester);
    }
}

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
                // Per Fase 1: només mock
                customWrite(1, YELLOW "Command OK\n" RESET);
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
            // Mock per Fase 1
            customWrite(1, MAGENTA "--- Alliance Status ---\n" RESET);
            if (maester->numAlliances == 0) {
                customWrite(1, YELLOW "No alliances established.\n" RESET);
            } else {
                for (int i = 0; i < maester->numAlliances; i++) {
                    char *msg;
                    const char *status_str;
                    switch (maester->alliances[i].status) {
                        case 1: status_str = "PENDING"; break;
                        case 2: status_str = "ALLIED"; break;
                        case 3: status_str = "FAILED"; break;
                        default: status_str = "NONE"; break;
                    }
                    asprintf(&msg, "  - %s: %s\n", maester->alliances[i].name, status_str);
                    customWrite(1, msg);
                    free(msg);
                }
            }
        } 
        else if (strcasecmp(tokens[1], "RESPOND") == 0) {
            if (count < 4) {
                customWrite(1, YELLOW "Did you mean to respond to a pledge? Please review syntax.\n" RESET);
                customWrite(1, CYAN "Usage: PLEDGE RESPOND <realm> <ACCEPT|REJECT>\n" RESET);
            } else if (count == 4 && (strcasecmp(tokens[3], "ACCEPT") == 0 || strcasecmp(tokens[3], "REJECT") == 0)) {
                customWrite(1, YELLOW "Command OK\n" RESET);
            } else {
                customWrite(1, RED "Unknown command\n" RESET);
                customWrite(1, CYAN "Usage: PLEDGE RESPOND <realm> <ACCEPT|REJECT>\n" RESET);
            }
        } 
        else if (count == 3) {
            // PLEDGE <realm> <sigil> - Mock per Fase 1
            customWrite(1, YELLOW "Command OK\n" RESET);
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
            // Mock per Fase 1
            customWrite(1, MAGENTA "--- Envoy Status ---\n" RESET);
            for (int i = 0; i < maester->envoys; i++) {
                char *msg;
                asprintf(&msg, "  - Envoy %d: FREE\n", i + 1);
                customWrite(1, msg);
                free(msg);
            }
        } 
        else {
            customWrite(1, RED "Unknown command\n" RESET);
            customWrite(1, CYAN "Usage: ENVOY STATUS\n" RESET);
        }
    }
    // EXIT
    else if (strcasecmp(tokens[0], "EXIT") == 0) {
        if (count == 1) {
            char *msg;
            asprintf(&msg, GREEN "The Maester of %s signs off. The ravens rest.\n" RESET, maester->name);
            customWrite(1, msg);
            free(msg);
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

void listRealms(Maester *maester) {
    customWrite(1, MAGENTA "--- Known Realms ---\n" RESET);
    
    int found = 0;
    for (int i = 0; i < maester->numRoutes; i++) {
        if (strcasecmp(maester->routes[i].name, "DEFAULT") == 0) {
            continue;
        }
        
        char *msg;
        asprintf(&msg, "  %s%s%s -> %s:%d\n", 
                CYAN, maester->routes[i].name, RESET,
                maester->routes[i].ip, maester->routes[i].port);
        customWrite(1, msg);
        free(msg);
        found = 1;
    }
    
    if (!found) {
        customWrite(1, YELLOW "No realms configured.\n" RESET);
    }
}

void listInventory(Maester *maester) {
    if (!maester || maester->numProducts == 0) {
        customWrite(1, RED "No trade goods available.\n" RESET);
        return;
    }

    customWrite(1, MAGENTA "--- Trade Ledger ---\n" RESET);
    customWrite(1, YELLOW "Item                      | Value (Gold) | Weight (Stone)\n" RESET);
    customWrite(1, YELLOW "--------------------------------------------------------\n" RESET);

    for (int i = 0; i < maester->numProducts; i++) {
        char *msg;
        asprintf(&msg, "%s%-25s | %-12d | %-12.1f%s\n", 
                CYAN, 
                maester->inventory[i].name, 
                maester->inventory[i].amount, 
                maester->inventory[i].weight, 
                RESET);
        customWrite(1, msg);
        free(msg);
    }

    customWrite(1, YELLOW "--------------------------------------------------------\n" RESET);
    
    char *msg;
    asprintf(&msg, "%sTotal Entries: %d%s\n", GREEN, maester->numProducts, RESET);
    customWrite(1, msg);
    free(msg);
}

void displayAvailableProducts(Maester *maester) {
    if (!maester || maester->numProducts == 0) {
        customWrite(1, YELLOW "No products available.\n" RESET);
        return;
    }

    customWrite(1, YELLOW "Available products: " RESET);
    for (int i = 0; i < maester->numProducts; i++) {
        char *msg;
        asprintf(&msg, "%s%s%s", CYAN, maester->inventory[i].name, RESET);
        customWrite(1, msg);
        free(msg);

        if (i < maester->numProducts - 1) {
            customWrite(1, YELLOW ", " RESET);
        }
    }
    customWrite(1, ".\n");
}

int isProductInInventory(char *productName, Maester *maester) {
    if (!productName || !maester) return 0;
    
    for (int i = 0; i < maester->numProducts; i++) {
        if (strcasecmp(maester->inventory[i].name, productName) == 0) {
            // Copiar el nom exacte (case-sensitive) del producte
            strcpy(productName, maester->inventory[i].name);
            return 1;
        }
    }
    return 0;
}

int checkRealm(Trade *trade, Maester *maester) {
    if (!trade || !maester) return 0;
    
    for (int i = 0; i < maester->numRoutes; i++) {
        if (strcasecmp(trade->kingdom, maester->routes[i].name) == 0) {
            // Copiar el nom exacte del realm
            strcpy(trade->kingdom, maester->routes[i].name);
            return 1;
        }
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

int handleSendCommand(Trade *trade) {
    if (!trade) return 0;
    
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
        char *msg;
        asprintf(&msg, GREEN "Trade list sent to %s.\n" RESET, trade->kingdom);
        customWrite(1, msg);
        free(msg);
        success = 1;
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

    if (!isProductInInventory(productName, maester)) {
        customWrite(1, RED "Product not in inventory.\n" RESET);
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

    asprintf(&msg, "%sEntering trade mode with %s.%s\n", 
            MAGENTA, trade->kingdom, RESET);
    customWrite(1, msg);
    free(msg);

    displayAvailableProducts(maester);

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
            exit = handleSendCommand(trade);
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
                    asprintf(&msg, "  %d x %s\n", 
                            trade->products[i].amount, 
                            trade->products[i].name);
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

void freeTrade(Trade **trade) {
    if (!trade || !*trade) return;
    
    Trade *t = *trade;
    
    if (t->kingdom) {
        free(t->kingdom);
        t->kingdom = NULL;
    }
    
    if (t->products) {
        for (int i = 0; i < t->numProducts; i++) {
            if (t->products[i].name) {
                free(t->products[i].name);
            }
        }
        free(t->products);
        t->products = NULL;
    }
    
    free(t);
    *trade = NULL;
}