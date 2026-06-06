#include "dataStructures.h"
#include "ipc.h"

void initMaester(Maester* m) {
    m->name = NULL;
    m->path = NULL;
    m->envoys = 0;
    m->ip = NULL;
    m->port = 0;
    m->inventory = NULL;
    m->numProducts = 0;
    m->routes = NULL;
    m->numRoutes = 0;
    m->alliances = NULL;
    m->numAlliances = 0;
    m->running = 1;
    m->serverSocket = -1;
    m->envoysAvailable = NULL;
    m->envoyMissions = NULL;
    m->remoteCatalogs = NULL;
    m->numRemoteCatalogs = 0;
    m->workersInfo = malloc(sizeof(WorkersInfo));
    m->workersInfo->numWorkers = 0; 
    m->workersInfo->workersThreadID = malloc(4*sizeof(pthread_t));
    
    pthread_mutex_init(&m->routes_mutex, NULL);
    pthread_mutex_init(&m->alliances_mutex, NULL);
    pthread_mutex_init(&m->inventory_mutex, NULL);
    pthread_mutex_init(&m->workersInfo->workers_mutex, NULL);
}

static void freeRemoteCatalog(RemoteCatalog *catalog) {
    if (!catalog) {
        return;
    }

    safeFree((void **)&catalog->realm);
    if (catalog->products) {
        for (int i = 0; i < catalog->numProducts; i++) {
            safeFree((void **)&catalog->products[i]);
        }
        free(catalog->products);
        catalog->products = NULL;
    }
    catalog->numProducts = 0;
}

void initRoute(Route *r) {
    r->name = NULL;
    r->ip = NULL;
    r->port = 0;
}

int readConfigFile(char *filename, Maester *maester) {
    int fd = open(filename, O_RDONLY);

    if (fd < 0) {
        return 1;
    }
    //Init maester struct
    initMaester(maester);

    //Read name var
    customRead(fd, &(maester->name), '\n');

    //Read path var
    customRead(fd, &(maester->path), '\n');

    //Read envoys number var
    char *aux = NULL;
    customRead(fd, &aux, '\n');
    maester->envoys = atoi(aux);
    safeFree((void**)&aux);
    
    //Read ip var
    customRead(fd, &(maester->ip), '\n');

    //Read port var
    customRead(fd, &aux, '\n');
    maester->port = atoi(aux);
    safeFree((void**)&aux);

    //Read useless line
    customRead(fd, &aux, '\n');
    safeFree((void**)&aux);

    //Read routes
    int eof = customRead(fd, &aux, ' ');

    while (eof) {
        Route* t = realloc(maester->routes, sizeof(Route)*(maester->numRoutes+1));
        if (!t) {
            safeFree((void**)&aux);
            close(fd);
            return 1;
        }
        maester->routes = t;

        //Init route
        initRoute(&(maester->routes[maester->numRoutes]));

        //Remove & if exists 
        removeChar(aux, '&');
        maester->routes[maester->numRoutes].name = strdup(aux);
        if (!maester->routes[maester->numRoutes].name) {
            safeFree((void**)&aux);
            close(fd);
            return 1;
        }
        safeFree((void**)&aux);

        //Read ip var
        customRead(fd, &maester->routes[maester->numRoutes].ip, ' ');

        //Read port var
        customRead(fd, &aux, '\n');
        maester->routes[maester->numRoutes].port = atoi(aux);
        safeFree((void**)&aux);

        maester->numRoutes++;

        //Read next line if exists
        eof = customRead(fd, &aux, ' ');
    }

    safeFree((void**)&aux);
    close(fd);

    return 0;
}


int readProducts(char *filename, Maester *maester) {
    if (!filename || !maester) return 1;

    int fd = open(filename, O_RDONLY);
    if (fd < 0) return 1;

    // Inicialitzar inventari buit
    maester->inventory = NULL;
    maester->numProducts = 0;

    AuxiliarProduct aux;
    ssize_t bytesRead;

    while ((bytesRead = read(fd, &aux, sizeof(AuxiliarProduct))) > 0) {
        // Verificar lectura completa del registre
        if (bytesRead != sizeof(AuxiliarProduct)) {
            // Lectura parcial - fitxer corrupte o incomplet
            for (int j = 0; j < maester->numProducts; j++) {
                free(maester->inventory[j].name);
            }
            free(maester->inventory);
            maester->inventory = NULL;
            maester->numProducts = 0;
            close(fd);
            return 1;
        }

        // Ampliar array de productes
        Product *tmp = realloc(maester->inventory, (maester->numProducts + 1) * sizeof(Product));
        if (!tmp) {
            for (int j = 0; j < maester->numProducts; j++) {
                free(maester->inventory[j].name);
            }
            free(maester->inventory);
            maester->inventory = NULL;
            maester->numProducts = 0;
            close(fd);
            return 1;
        }
        maester->inventory = tmp;

        // Copiar dades a Product dinàmic
        maester->inventory[maester->numProducts].amount = aux.amount;
        maester->inventory[maester->numProducts].weight = aux.weight;

        // Reservar memòria per al nom
        maester->inventory[maester->numProducts].name = malloc(strlen(aux.name) + 1);
        if (!maester->inventory[maester->numProducts].name) {
            for (int j = 0; j < maester->numProducts; j++) {
                free(maester->inventory[j].name);
            }
            free(maester->inventory);
            maester->inventory = NULL;
            maester->numProducts = 0;
            close(fd);
            return 1;
        }
        strcpy(maester->inventory[maester->numProducts].name, aux.name);

        maester->numProducts++;
    }

    // Verificar si hi ha hagut error de lectura (bytesRead < 0)
    if (bytesRead < 0) {
        for (int j = 0; j < maester->numProducts; j++) {
            free(maester->inventory[j].name);
        }
        free(maester->inventory);
        maester->inventory = NULL;
        maester->numProducts = 0;
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

void destroyMaester(Maester *maester) {
    if (!maester) {
        return;
    }

    // Free maester info
    safeFree((void**)&maester->name);
    safeFree((void**)&maester->path);
    safeFree((void**)&maester->ip);

    // Free inventory
    for (int i = 0; i < maester->numProducts; i++) {
        safeFree((void**)&maester->inventory[i].name);
    }
    safeFree((void**)&maester->inventory);

    // Free routes
    for (int i = 0; i < maester->numRoutes; i++) {
        safeFree((void**)&maester->routes[i].name);
        safeFree((void**)&maester->routes[i].ip);
    }
    safeFree((void**)&maester->routes);

    // Free alliances
    for (int i = 0; i < maester->numAlliances; i++) {
        safeFree((void**)&maester->alliances[i].name);
        safeFree((void**)&maester->alliances[i].ip);
    }
    safeFree((void**)&maester->alliances);
    
    // Destroy synchronization primitives
    pthread_mutex_destroy(&maester->routes_mutex);
    pthread_mutex_destroy(&maester->alliances_mutex);
    pthread_mutex_destroy(&maester->inventory_mutex);
    pthread_mutex_destroy(&maester->workersInfo->workers_mutex);
  
    safeFree((void**)&maester->workersInfo->workersThreadID);
    safeFree((void**)&maester->workersInfo);
    safeFree((void**)&maester->envoysAvailable);
    if (maester->envoyMissions) {
        for (int i = 0; i < maester->envoys; i++) {
            safeFree((void **)&maester->envoyMissions[i]);
        }
        free(maester->envoyMissions);
    }
    if (maester->remoteCatalogs) {
        for (int i = 0; i < maester->numRemoteCatalogs; i++) {
            freeRemoteCatalog(&maester->remoteCatalogs[i]);
        }
        free(maester->remoteCatalogs);
    }

    
    free(maester);

    return;
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

void destroyEnvoys(Maester *maester) {
    for (int i = 0; i < maester->envoys; i++) {
        free(maester->envoyPInfo.p2c[i]);
        free(maester->envoyPInfo.c2p[i]);
    }

    free(maester->envoyPInfo.p2c);
    free(maester->envoyPInfo.c2p);
    free(maester->envoyPInfo.envoyPIDs);
}

RemoteCatalog *findRemoteCatalog(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return NULL;
    }

    for (int i = 0; i < maester->numRemoteCatalogs; i++) {
        if (strcasecmp(maester->remoteCatalogs[i].realm, realmName) == 0) {
            return &maester->remoteCatalogs[i];
        }
    }

    return NULL;
}

int updateRemoteCatalog(Maester *maester, const char *realmName, const char *serializedProducts) {
    if (!maester || !realmName || !serializedProducts) {
        return -1;
    }

    RemoteCatalog *catalog = findRemoteCatalog(maester, realmName);
    if (!catalog) {
        RemoteCatalog *tmp = realloc(maester->remoteCatalogs, sizeof(RemoteCatalog) * (maester->numRemoteCatalogs + 1));
        if (!tmp) {
            return -1;
        }
        maester->remoteCatalogs = tmp;
        catalog = &maester->remoteCatalogs[maester->numRemoteCatalogs];
        catalog->realm = strdup(realmName);
        catalog->products = NULL;
        catalog->numProducts = 0;
        maester->numRemoteCatalogs++;
    } else {
        freeRemoteCatalog(catalog);
        catalog->realm = strdup(realmName);
    }

    char *copy = strdup(serializedProducts);
    char *cursor = copy;
    char *token = NULL;

    while (copy && (token = strsep(&cursor, "|")) != NULL) {
        if (*token == '\0') {
            continue;
        }
        char **products = realloc(catalog->products, sizeof(char *) * (catalog->numProducts + 1));
        if (!products) {
            free(copy);
            return -1;
        }
        catalog->products = products;
        catalog->products[catalog->numProducts] = strdup(token);
        if (!catalog->products[catalog->numProducts]) {
            free(copy);
            return -1;
        }
        catalog->numProducts++;
    }

    free(copy);
    return 0;
}

void endAndCleanEnvoys(Maester *maester) {
    for (int i = 0; i < maester->envoys; i++) {
        IpcRequest request;
        memset(&request, 0, sizeof(IpcRequest));
        request.type = IPC_SHUTDOWN;
        sendIpcRequest(maester->envoyPInfo.p2c[i][1], &request);
    }

    for (int i = 0; i < maester->envoys; i++) {
        waitpid(maester->envoyPInfo.envoyPIDs[i], NULL, 0);
        close(maester->envoyPInfo.p2c[i][1]);
        close(maester->envoyPInfo.c2p[i][0]);
        free(maester->envoyPInfo.p2c[i]);
        free(maester->envoyPInfo.c2p[i]);
    }

    free(maester->envoyPInfo.p2c);
    free(maester->envoyPInfo.c2p);
    free(maester->envoyPInfo.envoyPIDs);
}
