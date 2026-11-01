#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H

// ============================================================
//  Déclarations PARTAGÉES entre les fichiers de src/client/ — un
//  fichier par mode (client_list.c pour -list, puis client_create.c,
//  client_browse.c, client_extract.c à venir), et vsh.c qui ne fait QUE
//  le parsing de argv, la connexion au serveur, et l'aiguillage vers la
//  bonne fonction ci-dessous selon le mode demandé. Même principe que
//  server_internal.h côté serveur.
//
//  Pas de "client.h" public dans include/ : contrairement à archive.h,
//  rien EN DEHORS de src/client/ n'a jamais besoin d'appeler une de ces
//  fonctions — server.c ne les connaît pas, et ne les appelle jamais
//  directement non plus (la communication passe uniquement par le
//  socket réseau, voir vsh.c).
// ============================================================

int run_list_command(int server_socket);
// Implémente le mode -list côté client : envoie la commande LIST sur
// server_socket (déjà connecté), lit la réponse selon le protocole, et
// affiche la liste des archives sur stdout. Retourne 0 si succès, 1 en
// cas d'erreur réseau ou si le serveur a répondu ERR.

#endif