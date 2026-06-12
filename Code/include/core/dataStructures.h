#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>

#include "utils.h"

#define ALLIANCE_NONE 0
#define ALLIANCE_PENDING 1
#define ALLIANCE_ACTIVE 2
#define ALLIANCE_FAILED 3

#define ALLIANCE_TIMEOUT_SECONDS 120  // 2 minuts

#define MAX_COMMAND_LENGTH 512
#define WORKERS_CAPACITY 4

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
    int status;
    time_t requestTime;
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
    char *realm;
    char **products;
    float *weights;     // pes per unitat de cada producte (paral·lel a products)
    int numProducts;
} RemoteCatalog;

typedef struct {
    int **p2c;
    int **c2p;
    pid_t *envoyPIDs;
} EnvoyPInfo;

typedef struct {
    pthread_t *workersThreadID;
    int numWorkers;
    pthread_mutex_t workers_mutex;
} WorkersInfo;

typedef struct {
    // Maester information
    char *name;
    char *path;        // carpeta de fitxers de l'usuari (2a línia del .dat)
    char *stockFile;   // ruta del fitxer d'inventari binari (argv[2]), per persistir
    int envoys;
    EnvoyPInfo envoyPInfo;
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

    //Workers info
    WorkersInfo *workersInfo;

    volatile sig_atomic_t running;

    int serverSocket;

    // Synchronization
    pthread_mutex_t routes_mutex;      // Protects routes[]
    pthread_mutex_t alliances_mutex;   // Protects alliances[]
    pthread_mutex_t inventory_mutex;   // Protects inventory[]

    int *envoysAvailable;            
    char **envoyMissions;

    RemoteCatalog *remoteCatalogs;
    int numRemoteCatalogs;
    
} Maester;

typedef struct {
    int p2c;
    int c2p;
} Envoy;

int readConfigFile(char *filename, Maester *maester);

int readProducts(char *filename, Maester *maester);

void destroyMaester(Maester *maester);

void freeTrade(Trade **trade);

void destroyEnvoys(Maester *maester);

void endAndCleanEnvoys(Maester *maester);

RemoteCatalog *findRemoteCatalog(Maester *maester, const char *realmName);

int updateRemoteCatalog(Maester *maester, const char *realmName, const char *serializedProducts);

float remoteCatalogWeight(Maester *maester, const char *realmName, const char *productName);

int decrementInventory(Maester *maester, const char *productName, int quantity);

int incrementInventory(Maester *maester, const char *productName, int quantity, float weight);

int updateStockDB(const char *filename, Maester *maester);

#endif
