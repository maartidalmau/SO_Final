#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <signal.h>
#include "utils.h"

#define MAX_TOKENS 10  // Augmentat per noms de productes amb espais
#define MAX_COMMAND_LENGTH 512

typedef struct {
    char name[100];
    int amount;
    float weight;
} AuxiliarProduct;

typedef struct {
    char *name;
    int amount;
    float weight;
} Product;

typedef struct {
    char *name;
    char *ip;
    int port;
} Route;

typedef struct {
    char *name;
    char *ip;
    int port;
} Alliance;

typedef struct {
    char *name;
    int amount;
} TradeProduct;

typedef struct {
    char *kingdom;
    TradeProduct *products;
    int numProducts;
    int totalAmount;
} Trade;

typedef struct {
    // Maester information
    char *name;
    char *path;
    int envoys;
    char *ip;
    int port;

    // Inventory of products
    Product *inventory;
    int numProducts;

    // Routing table
    Route *routes;
    int numRoutes;

    // Alliances
    Alliance *alliances;
    int numAlliances;

    volatile sig_atomic_t running;
} Maester;


int readConfigFile(char *filename, Maester *maester);

#endif