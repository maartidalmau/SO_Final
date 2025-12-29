#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

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


} Maester;

#endif