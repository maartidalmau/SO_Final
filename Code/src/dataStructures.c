#include "dataStructures.h"

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
    m->workersInfo = malloc(sizeof(WorkersInfo));
    m->workersInfo->numWorkers = 0; 
    m->workersInfo->workersThreadID = malloc(4*sizeof(pthread_t));
    
    pthread_mutex_init(&m->routes_mutex, NULL);
    pthread_mutex_init(&m->alliances_mutex, NULL);
    pthread_mutex_init(&m->inventory_mutex, NULL);
    pthread_mutex_init(&m->workersInfo->workers_mutex, NULL);
    
    SEM_constructor(&m->envoys_sem);
    SEM_init(&m->envoys_sem, 1);
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
    
    // Initialize envoys semaphore with the number of available envoys
    sem_init(&maester->envoys_sem, 0, maester->envoys);

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
    sem_destroy(&maester->envoys_sem);

    pthread_mutex_destroy(&maester->workersInfo->workers_mutex);
    safeFree((void**)&maester->workersInfo->workersThreadID);
    safeFree((void**)&maester->workersInfo);

    SEM_destructor(&maester->envoys_sem);
    
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

void endAndCleanEnvoys(Maester *maester) {
    for (int i = 0; i<maester->envoys;i++){
        kill(maester->envoyPInfo.envoyPIDs[i], SIGUSR1);
    }

    for (int i = 0; i < maester->envoys; i++) {
        waitpid(maester->envoyPInfo.envoyPIDs[i], NULL, 0);
        free(maester->envoyPInfo.p2c[i]);
        free(maester->envoyPInfo.c2p[i]);
    }

    free(maester->envoyPInfo.p2c);
    free(maester->envoyPInfo.c2p);
    free(maester->envoyPInfo.envoyPIDs);
}
