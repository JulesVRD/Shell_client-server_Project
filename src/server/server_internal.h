#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

// ============================================================
//  Déclarations PARTAGÉES entre les fichiers de src/server/ — un
//  fichier par commande (server_list.c pour LIST, puis server_browse.c,
//  server_create.c, server_extract.c à venir), et server.c qui ne fait
//  QUE le squelette réseau (socket, boucle d'acceptation) et
//  l'aiguillage vers la bonne fonction ci-dessous selon la commande
//  reçue. Même principe que archive_internal.h dans common/ : jamais
//  utilisé par vsh.c, qui n'a rien à voir avec l'implémentation du
//  serveur.
// ============================================================

void handle_list_command(int client_socket, const char *archives_directory_path);
// Implémente le mode LIST : liste les fichiers ".arch" du dossier
// archives_directory_path et répond au client selon le protocole
// (OK, nombre d'archives, puis une ligne par archive — ou ERR si le
// dossier est inaccessible). Voir server_list.c.

#endif