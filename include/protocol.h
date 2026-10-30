#ifndef PROTOCOL_H
#define PROTOCOL_H
//pour eviter que protocol.h soit inclus plusieurs fois dans le meme fichier source


// ============================================================
//  Protocole applicatif entre vsh (client) et server.
//  Ce fichier est #include par les DEUX côtés : c'est le "contrat"
//  qui garantit qu'ils se comprennent. Toute modification ici doit
//  être recompilée des deux côtés.
// ============================================================

#define PROTO_MAX_LINE 1024   // taille max d'une ligne de commande/réponse

// --- Commandes envoyées par le client (1 ligne, "MOT_CLE arguments") ---
#define CMD_LIST    "LIST"      // LIST                 -> réponse: liste des archives
#define CMD_CREATE  "CREATE"    // CREATE <nom>          -> puis <taille>\n + octets de l'archive
#define CMD_BROWSE  "BROWSE"    // BROWSE <nom>          -> ouvre une session interactive
#define CMD_EXTRACT "EXTRACT"   // EXTRACT <nom>         -> réponse: <taille>\n + octets de l'archive

// --- Sous-commandes valables UNIQUEMENT pendant une session BROWSE ---
#define CMD_PWD   "pwd"
#define CMD_LS    "ls"
#define CMD_CD    "cd"
#define CMD_CAT   "cat"
#define CMD_RM    "rm"
#define CMD_TOUCH "touch"
#define CMD_MKDIR "mkdir"
#define CMD_EXIT  "exit"        // pas dans le sujet, mais indispensable pour sortir du mode browse !

// --- Codes de réponse génériques (1ère ligne de chaque réponse serveur) ---
#define RESP_OK  "OK"
#define RESP_ERR "ERR"

#endif