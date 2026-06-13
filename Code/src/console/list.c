#include "list.h"

void listRealms(Maester *maester) {
    customWrite(1, MAGENTA "--- Known Realms ---\n" RESET);
    
    int found = 0;
    for (int i = 0; i < maester->numRoutes; i++) {
        if (strcasecmp(maester->routes[i].name, "DEFAULT") == 0) {
            continue;
        }
        
        char *msg;
        asprintf(&msg, BLUE "\t- %s\n" RESET, maester->routes[i].name);
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
    customWrite(1, YELLOW "Item                           | Value (Gold) | Weight (Stone)\n" RESET);
    customWrite(1, YELLOW "------------------------------------------------------------------\n" RESET);

    for (int i = 0; i < maester->numProducts; i++) {
        char *msg;
        asprintf(&msg, "%s%-30s | %-12d | %-12.1f%s\n", 
                CYAN, 
                maester->inventory[i].name, 
                maester->inventory[i].amount, 
                maester->inventory[i].weight, 
                RESET);
        customWrite(1, msg);
        free(msg);
    }

    customWrite(1, YELLOW "------------------------------------------------------------------\n" RESET);

    char *msg;
    asprintf(&msg, "%sTotal Entries: %d%s\n", GREEN, maester->numProducts, RESET);
    customWrite(1, msg);
    free(msg);
}

// Mostra l'inventari d'un regne aliat (rebut en un buffer en MEMÒRIA de registres
// AuxiliarProduct) amb el MATEIX format que l'inventari propi, titulat amb el regne.
void listRemoteInventoryBuf(const char *realmName, const uint8_t *buf, unsigned long len) {
    char *msg;
    asprintf(&msg, MAGENTA "--- Trade Ledger of %s ---\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    customWrite(1, YELLOW "Item                           | Value (Gold) | Weight (Stone)\n" RESET);
    customWrite(1, YELLOW "------------------------------------------------------------------\n" RESET);

    long count = (buf && len) ? (long)(len / sizeof(AuxiliarProduct)) : 0;
    for (long i = 0; i < count; i++) {
        AuxiliarProduct aux;
        memcpy(&aux, buf + (unsigned long)i * sizeof(AuxiliarProduct), sizeof(AuxiliarProduct));
        aux.name[sizeof(aux.name) - 1] = '\0';
        asprintf(&msg, "%s%-30s | %-12d | %-12.1f%s\n",
                 CYAN, aux.name, aux.amount, aux.weight, RESET);
        customWrite(1, msg);
        free(msg);
    }

    customWrite(1, YELLOW "------------------------------------------------------------------\n" RESET);
    asprintf(&msg, "%sTotal Entries: %ld%s\n", GREEN, count, RESET);
    customWrite(1, msg);
    free(msg);
}