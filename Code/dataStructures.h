#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include "utils.h"

#define ALLIANCE_NONE 0
#define ALLIANCE_PENDING 1
#define ALLIANCE_ACTIVE 2
#define ALLIANCE_FAILED 3

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
    int status; // 0: inactive, 1: active
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

    int serverSocket;

    // Synchronization
    pthread_mutex_t routes_mutex;      // Protects routes[]
    pthread_mutex_t alliances_mutex;   // Protects alliances[]
    pthread_mutex_t inventory_mutex;   // Protects inventory[]
    sem_t envoys_sem;                  // Limits concurrent outgoing connections
} Maester;


int readConfigFile(char *filename, Maester *maester);

int readProducts(char *filename, Maester *maester);

void destroyMaester(Maester *maester);

void freeTrade(Trade **trade);

#endif