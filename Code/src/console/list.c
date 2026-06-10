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

// Mostra l'inventari d'un regne aliat (rebut com a fitxer binari d'AuxiliarProduct)
// amb el MATEIX format que l'inventari propi, però titulat amb el nom del regne.
void listRemoteInventory(const char *realmName, const char *filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        customWrite(1, RED "No products available from this realm.\n" RESET);
        return;
    }

    char *msg;
    asprintf(&msg, MAGENTA "--- Trade Ledger of %s ---\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    customWrite(1, YELLOW "Item                           | Value (Gold) | Weight (Stone)\n" RESET);
    customWrite(1, YELLOW "------------------------------------------------------------------\n" RESET);

    AuxiliarProduct aux;
    int count = 0;
    while (read(fd, &aux, sizeof(AuxiliarProduct)) == (ssize_t)sizeof(AuxiliarProduct)) {
        aux.name[sizeof(aux.name) - 1] = '\0';
        asprintf(&msg, "%s%-30s | %-12d | %-12.1f%s\n",
                 CYAN, aux.name, aux.amount, aux.weight, RESET);
        customWrite(1, msg);
        free(msg);
        count++;
    }
    close(fd);

    customWrite(1, YELLOW "------------------------------------------------------------------\n" RESET);
    asprintf(&msg, "%sTotal Entries: %d%s\n", GREEN, count, RESET);
    customWrite(1, msg);
    free(msg);
}