#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdint.h>

#include "utils.h"

#define ALLIANCE_NONE 0
#define ALLIANCE_PENDING 1
#define ALLIANCE_ACTIVE 2
#define ALLIANCE_FAILED 3

#define ALLIANCE_TIMEOUT_SECONDS 120  // 2 minuts

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
    int status;
    time_t requestTime;  // Timestamp de quan es va enviar/rebre la petició
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
    int **p2c;
    int **c2p;
    pid_t *envoyPIDs;
} EnvoyPInfo;

typedef struct {
    pthread_t *workersThreadID;
    int numWorkers;
    int workersCapacity;
    pthread_mutex_t workers_mutex;
}WorkersInfo;

typedef struct {
    // Maester information
    char *name;
    char *path;
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
    sem_t envoys_sem;                  // Limits concurrent outgoing connections
} Maester;

typedef struct {
    int p2c;
    int c2p;
    volatile sig_atomic_t running;
} Envoy;



int readConfigFile(char *filename, Maester *maester);

int readProducts(char *filename, Maester *maester);

void destroyMaester(Maester *maester);

void freeTrade(Trade **trade);

void destroyEnvoys(Maester *maester);

void endAndCleanEnvoys(Maester *maester);

#endif