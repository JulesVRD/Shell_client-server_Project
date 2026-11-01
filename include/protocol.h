#ifndef PROTOCOL_H
#define PROTOCOL_H

// ============================================================
//  Vocabulaire du protocole applicatif entre vsh (client) et server.
//  Ce fichier est #include par les DEUX côtés : c'est le "contrat" qui
//  garantit qu'ils se comprennent. Toute modification ici doit être
//  recompilée des deux côtés avant de retester quoi que ce soit.
// ============================================================

#define MAX_PROTOCOL_LINE_LENGTH 1024
// Taille maximale, en octets, d'une ligne de commande ou de réponse
// échangée sur le protocole (ex: "LIST", "cd A", "OK"). Sert à
// dimensionner tous les buffers passés à receive_line() dans vsh.c et
// server.c, pour que cette limite soit cohérente partout dans le projet
// plutôt que recopiée à la main à chaque appel.

#define CLIENT_COMMAND_LIST "LIST"
// Envoyée par le client en mode -list. Aucun argument : demande au
// serveur la liste des archives qu'il héberge.

#define CLIENT_COMMAND_CREATE "CREATE"
// Envoyée par le client en mode -create, suivie du nom de l'archive à
// créer (ex: "CREATE backup.arch"). Le serveur répond SERVER_RESPONSE_OK,
// puis le client envoie la taille en octets de l'archive suivie du
// contenu brut de l'archive elle-même.

#define CLIENT_COMMAND_BROWSE "BROWSE"
// Envoyée par le client en mode -browse, suivie du nom de l'archive à
// explorer. Après la réponse SERVER_RESPONSE_OK du serveur, la connexion
// entre dans une boucle de sous-commandes (voir plus bas), jusqu'à ce
// que le client envoie BROWSE_COMMAND_EXIT.

#define CLIENT_COMMAND_EXTRACT "EXTRACT"
// Envoyée par le client en mode -extract, suivie du nom de l'archive à
// extraire. Le serveur répond directement par la taille en octets puis
// le contenu brut de l'archive, sans étape OK intermédiaire.


// --- Sous-commandes valables UNIQUEMENT à l'intérieur d'une session
//     ouverte par CLIENT_COMMAND_BROWSE (jamais en dehors) -----------

#define BROWSE_COMMAND_PWD   "pwd"
// Demande au serveur le chemin absolu du répertoire courant dans
// l'archive en cours de navigation pour cette session.

#define BROWSE_COMMAND_LS    "ls"
// Demande au serveur la liste des fichiers/répertoires du répertoire
// courant (ou d'un chemin donné en argument de la commande).

#define BROWSE_COMMAND_CD    "cd"
// Demande un changement de répertoire courant côté serveur, pour cette
// session de browse précise (chaque session a son propre "répertoire
// courant", indépendant des autres clients connectés).

#define BROWSE_COMMAND_CAT   "cat"
// Demande le contenu d'un ou plusieurs fichiers du répertoire courant.

#define BROWSE_COMMAND_RM    "rm"
// Demande la suppression d'un fichier, ou d'un répertoire et de tout
// son contenu, dans l'arbre ArchiveNode en mémoire côté serveur.

#define BROWSE_COMMAND_TOUCH "touch"
// Demande la création d'un fichier vide dans le répertoire courant.

#define BROWSE_COMMAND_MKDIR "mkdir"
// Demande la création d'un répertoire dans le répertoire courant.

#define BROWSE_COMMAND_EXIT  "exit"
// Met fin proprement à la session de browse. Absente de l'énoncé mais
// indispensable : sans elle, impossible de sortir du mode browse et de
// fermer la connexion sans que ce soit traité comme une erreur réseau.

// --- Première ligne de CHAQUE réponse envoyée par le serveur --------

#define SERVER_RESPONSE_OK  "OK"
// Indique que la commande précédente du client s'est déroulée sans
// erreur. Peut être suivie d'autres lignes de données selon la
// commande (ex: la liste des fichiers juste après un "ls").

#define SERVER_RESPONSE_ERR "ERR"
// Indique que la commande précédente a échoué (ex: "cd" vers un
// fichier, fichier introuvable pour "cat"). Généralement suivie d'un
// message d'erreur en texte sur la ligne suivante.

#endif