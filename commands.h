#ifndef COMMANDS_H
#define COMMANDS_H

#include "dataStructures.h"

// Main console logic
void consoleLogic(Maester *maester);
int commandHandler(char **command, Maester *maester);

// Command implementations
void listRealms(Maester *maester);
void listInventory(Maester *maester);
void startTrade(Trade *trade, Maester *maester);

// Helper functions
void displayAvailableProducts(Maester *maester);
int isProductInInventory(char *productName, Maester *maester);
int checkRealm(Trade *trade, Maester *maester);

// Trade functions
int findTradeProduct(Trade *trade, char *productName);
int addProductToTrade(Trade *trade, char *productName, int quantity);
int removeProductFromTrade(Trade *trade, char *productName, int quantity);
char* generateTradeFileName(Trade *trade);
int writeTradeFile(Trade *trade, char *fileName);

// Trade command handlers
int handleSendCommand(Trade *trade);
int handleAddCommand(Trade *trade, char *productName, int quantity, Maester *maester);
int handleRemoveCommand(Trade *trade, char *productName, int quantity);
char* buildProductName(char *tokens[], int count);

// Cleanup
void freeTrade(Trade **trade);

#endif