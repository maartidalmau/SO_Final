#include "list.h"

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