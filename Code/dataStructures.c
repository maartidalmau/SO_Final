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
    
    // Initialize synchronization primitives
    pthread_mutex_init(&m->routes_mutex, NULL);
    pthread_mutex_init(&m->alliances_mutex, NULL);
    pthread_mutex_init(&m->inventory_mutex, NULL);
    // envoys_sem will be initialized after reading config (with m->envoys value)
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
        //Realloc
        Route* t = realloc(maester->routes, sizeof(Route)*(maester->numRoutes+1));
        maester->routes = t;

        //Init route
        initRoute(&(maester->routes[maester->numRoutes]));

        //Remove & if exists 
        removeChar(aux, '&');
        maester->routes[maester->numRoutes].name = strdup(aux);
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


//SHA DE CANVIAR
int readProducts(char *filename, Maester *maester) {
    if (!filename || !maester) return 1;

    int fd = open(filename, O_RDONLY);
    if (fd < 0) return 1;

    // Obtener tamaño del archivo
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return 1;
    }

    int numProducts = st.st_size / sizeof(AuxiliarProduct);
    if (numProducts <= 0) {
        maester->inventory = NULL;
        maester->numProducts = 0;
        close(fd);
        return 0;
    }

    // Reservar memoria para los productos
    maester->inventory = malloc(numProducts * sizeof(Product));
    if (!maester->inventory) {
        close(fd);
        return 1;
    }
    maester->numProducts = numProducts;

    AuxiliarProduct aux;
    for (int i = 0; i < numProducts; i++) {
        ssize_t r = read(fd, &aux, sizeof(AuxiliarProduct));
        if (r != sizeof(AuxiliarProduct)) {
            // Error de lectura: liberar lo reservado
            for (int j = 0; j < i; j++) free(maester->inventory[j].name);
            free(maester->inventory);
            maester->inventory = NULL;
            maester->numProducts = 0;
            close(fd);
            return 1;
        }

        // Copiar datos a Product dinámico
        maester->inventory[i].amount = aux.amount;
        maester->inventory[i].weight = aux.weight;

        // Reservar memoria para el string
        maester->inventory[i].name = malloc(strlen(aux.name) + 1);
        if (!maester->inventory[i].name) {
            for (int j = 0; j < i; j++) free(maester->inventory[j].name);
            free(maester->inventory);
            maester->inventory = NULL;
            maester->numProducts = 0;
            close(fd);
            return 1;
        }
        strcpy(maester->inventory[i].name, aux.name);
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