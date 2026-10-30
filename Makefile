# ============================================================
#  Makefile — Projet LO14 : Serveur d'archives (client vsh + serveur)
# ============================================================

# --- Compilateur et options --------------------------------------------
CC     = gcc
# -Wall -Wextra : active (presque) tous les avertissements du compilateur,
#                 indispensable en C pour éviter les bugs silencieux
# -g            : garde les infos de debug dans le binaire (requis pour gdb/VSCode)
# -std=gnu11    : norme C11 + extensions GNU (on en aura besoin pour les sockets POSIX)
# -Iinclude     : dit à gcc de chercher les .h dans le dossier include/
CFLAGS = -Wall -Wextra -g -std=gnu11 -Iinclude

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# --- Sources communes (partagées client/serveur) ------------------------
COMMON_SRC = $(wildcard $(SRC_DIR)/common/*.c)
COMMON_OBJ = $(COMMON_SRC:$(SRC_DIR)/common/%.c=$(OBJ_DIR)/common/%.o)

# --- Sources du client vsh -----------------------------------------------
CLIENT_SRC = $(wildcard $(SRC_DIR)/client/*.c)
CLIENT_OBJ = $(CLIENT_SRC:$(SRC_DIR)/client/%.c=$(OBJ_DIR)/client/%.o)

# --- Sources du serveur ---------------------------------------------------
SERVER_SRC = $(wildcard $(SRC_DIR)/server/*.c)
SERVER_OBJ = $(SERVER_SRC:$(SRC_DIR)/server/%.c=$(OBJ_DIR)/server/%.o)

CLIENT_BIN = $(BIN_DIR)/vsh
SERVER_BIN = $(BIN_DIR)/server

# --- Cible par défaut : construit les deux exécutables --------------------
all: $(CLIENT_BIN) $(SERVER_BIN)

# --- Édition de liens -------------------------------------------------------
$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# --- Compilation (règles génériques .c -> .o) -------------------------------
# $< = le fichier source dépendant, $@ = la cible (le .o produit)
$(OBJ_DIR)/common/%.o: $(SRC_DIR)/common/%.c | $(OBJ_DIR)/common
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/client/%.o: $(SRC_DIR)/client/%.c | $(OBJ_DIR)/client
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/server/%.o: $(SRC_DIR)/server/%.c | $(OBJ_DIR)/server
	$(CC) $(CFLAGS) -c $< -o $@

# --- Création des dossiers si absents (dépendance "order-only" avec |) ------
$(BIN_DIR) $(OBJ_DIR)/common $(OBJ_DIR)/client $(OBJ_DIR)/server:
	mkdir -p $@

# --- Nettoyage ----------------------------------------------------------------
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# .PHONY : "all" et "clean" ne sont pas des fichiers réels, make ne doit
# jamais sauter leur exécution sous prétexte qu'un fichier du même nom existe
.PHONY: all clean