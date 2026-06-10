CFLAGS = -Wextra -Wall -MMD -MP -Iinclude -Iinclude/core -Iinclude/console -Iinclude/network
LDFLAGS = -lpthread

# Auto-generated header dependencies (.d files from -MMD).
# Garanteix que en editar qualsevol .h es recompilin tots els .o afectats.
-include $(wildcard *.d)

maester: maester.o dataStructures.o utils.o console_console.o console_list.o console_trade.o network_envoy.o network_frame.o network_network.o network_allianceHandler.o network_frameHandler.o network_server.o network_client.o network_router.o network_ipc.o semaphore_v2.o core_md5.o
	gcc maester.o dataStructures.o utils.o console_console.o console_list.o console_trade.o network_envoy.o network_frame.o network_network.o network_allianceHandler.o network_frameHandler.o network_server.o network_client.o network_router.o network_ipc.o semaphore_v2.o core_md5.o -o Maester $(CFLAGS) $(LDFLAGS)

maester.o: src/maester.c include/core/dataStructures.h include/core/utils.h include/console/console.h include/network/server.h include/network/client.h include/network/envoy.h
	gcc -c -g src/maester.c -o maester.o $(CFLAGS)

dataStructures.o: src/dataStructures.c include/core/dataStructures.h include/core/utils.h
	gcc -c -g src/dataStructures.c -o dataStructures.o $(CFLAGS)

utils.o: src/utils.c include/core/utils.h
	gcc -c -g src/utils.c -o utils.o $(CFLAGS)

console_console.o: src/console/console.c include/console/console.h include/core/utils.h include/core/dataStructures.h
	gcc -c -g src/console/console.c -o console_console.o $(CFLAGS)

console_list.o: src/console/list.c include/console/list.h include/core/dataStructures.h include/core/utils.h
	gcc -c -g src/console/list.c -o console_list.o $(CFLAGS)

console_trade.o: src/console/trade.c include/console/trade.h include/core/dataStructures.h include/core/utils.h
	gcc -c -g src/console/trade.c -o console_trade.o $(CFLAGS)

network_envoy.o: src/network/core/envoy.c include/network/envoy.h include/core/utils.h
	gcc -c -g src/network/core/envoy.c -o network_envoy.o $(CFLAGS)

network_frame.o: src/network/core/frame.c include/network/frame.h include/core/utils.h
	gcc -c -g src/network/core/frame.c -o network_frame.o $(CFLAGS)

network_network.o: src/network/core/network.c include/network/network.h include/core/utils.h
	gcc -c -g src/network/core/network.c -o network_network.o $(CFLAGS)

network_allianceHandler.o: src/network/handlers/allianceHandler.c include/network/allianceHandler.h include/core/dataStructures.h include/core/utils.h
	gcc -c -g src/network/handlers/allianceHandler.c -o network_allianceHandler.o $(CFLAGS)

network_frameHandler.o: src/network/handlers/frameHandler.c include/network/frameHandler.h include/network/frame.h include/core/utils.h
	gcc -c -g src/network/handlers/frameHandler.c -o network_frameHandler.o $(CFLAGS)

network_server.o: src/network/transport/server.c include/network/server.h include/network/network.h include/core/utils.h
	gcc -c -g src/network/transport/server.c -o network_server.o $(CFLAGS)

network_client.o: src/network/transport/client.c include/network/client.h include/network/network.h include/core/utils.h
	gcc -c -g src/network/transport/client.c -o network_client.o $(CFLAGS)

network_router.o: src/network/transport/router.c include/network/router.h include/network/network.h include/core/utils.h
	gcc -c -g src/network/transport/router.c -o network_router.o $(CFLAGS)

network_ipc.o: src/network/transport/ipc.c include/network/ipc.h include/network/network.h include/core/dataStructures.h
	gcc -c -g src/network/transport/ipc.c -o network_ipc.o $(CFLAGS)

semaphore_v2.o: src/semaphore_v2.c include/core/semaphore_v2.h
	gcc -c -g src/semaphore_v2.c -o semaphore_v2.o $(CFLAGS)

core_md5.o: src/core/md5.c include/core/md5.h
	gcc -c -g src/core/md5.c -o core_md5.o $(CFLAGS)

clean:
	rm -f *.o *.d Maester

valgrind: maester
	valgrind --leak-check=yes --track-fds=yes --show-reachable=yes --track-origins=yes --tool=memcheck ./Maester $(ARGS)

# GLOBAL TESTS
A: maester
	./Maester ../Configs/A-Dragonstone.dat ../Assets/stark_stock.db

B: maester
	./Maester ../Configs/B-KingsLanding.dat ../Assets/baratheon_stock.db

C: maester
	./Maester ../Configs/C-TheVale.dat ../Assets/arryn_stock.db

D: maester
	./Maester ../Configs/D-Driftmark.dat ../Assets/velaryon_stock.db

E: maester
	./Maester ../Configs/E-Riverrun.dat ../Assets/tully_stock.db

# GLOBBAL TEST with Valgrind
A-valgrind: maester
	valgrind --leak-check=yes --track-fds=yes --show-reachable=yes --track-origins=yes --tool=memcheck ./Maester ../Configs/A-Dragonstone.dat ../Assets/stark_stock.db

B-valgrind: maester
	valgrind --leak-check=yes --track-fds=yes --show-reachable=yes --track-origins=yes --tool=memcheck ./Maester ../Configs/B-KingsLanding.dat ../Assets/baratheon_stock.db

C-valgrind: maester
	valgrind --leak-check=yes --track-fds=yes --show-reachable=yes --track-origins=yes --tool=memcheck ./Maester ../Configs/C-TheVale.dat ../Assets/arryn_stock.db

D-valgrind: maester
	valgrind --leak-check=yes --track-fds=yes --show-reachable=yes --track-origins=yes --tool=memcheck ./Maester ../Configs/D-Driftmark.dat ../Assets/velaryon_stock.db

E-valgrind: maester
	valgrind --leak-check=yes --track-fds=yes --show-reachable=yes --track-origins=yes --tool=memcheck ./Maester ../Configs/E-Riverrun.dat ../Assets/tully_stock.db

.PHONY: clean valgrind A B C D E A-valgrind B-valgrind C-valgrind D-valgrind E-valgrind