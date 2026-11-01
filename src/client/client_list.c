// ============================================================
//  Implémentation du mode -list côté client. Symétrique de
//  server_list.c : là où le serveur listait un DOSSIER, le client ici
//  ne fait qu'ENVOYER une commande texte et AFFICHER ce qui revient —
//  aucune des deux fonctions n'appelle l'autre directement, elles ne
//  communiquent que via le socket (voir vsh.c pour la connexion).
// ============================================================

#include "client_internal.h"
#include "protocol.h"
#include "network_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int run_list_command(int server_socket)
{
    // Le protocole (protocol.h) dit : "LIST" sans argument. send_line
    // ajoute elle-même le '\n' final, pas besoin de l'inclure ici.
    if (send_line(server_socket, CLIENT_COMMAND_LIST) < 0) {
        fprintf(stderr, "Erreur reseau : impossible d'envoyer la commande LIST\n");
        return 1;
    }

    char response_line[MAX_PROTOCOL_LINE_LENGTH];

    // Première ligne attendue : SERVER_RESPONSE_OK ou SERVER_RESPONSE_ERR
    // (convention appliquée à CHAQUE réponse du serveur, voir protocol.h).
    int status_length = receive_line(server_socket, response_line, sizeof(response_line));
    if (status_length < 0) {
        // -1 : connexion fermée avant même ce premier mot — le serveur a
        // peut-être planté, ou n'a jamais reçu la commande correctement.
        fprintf(stderr, "Erreur reseau : le serveur n'a pas repondu\n");
        return 1;
    }

    if (strcmp(response_line, SERVER_RESPONSE_ERR) == 0) {
        // D'après server_list.c, une ligne de message suit toujours un
        // ERR — on la lit pour l'afficher à l'utilisateur plutôt que de
        // le laisser deviner ce qui a cloché côté serveur.
        int message_length = receive_line(server_socket, response_line, sizeof(response_line));
        if (message_length >= 0) {
            fprintf(stderr, "Erreur du serveur : %s\n", response_line);
        }
        return 1;
    }

    if (strcmp(response_line, SERVER_RESPONSE_OK) != 0) {
        // Ni OK ni ERR : le serveur ne respecte pas le protocole attendu
        // (version différente ? bug ?) — on le signale clairement plutôt
        // que de continuer à lire des données qui n'ont plus de sens.
        fprintf(stderr, "Reponse inattendue du serveur : '%s'\n", response_line);
        return 1;
    }

    // Deuxième ligne attendue : le nombre d'archives, en texte.
    int count_length = receive_line(server_socket, response_line, sizeof(response_line));
    if (count_length < 0) {
        fprintf(stderr, "Erreur reseau : nombre d'archives manquant\n");
        return 1;
    }

    // atoi() plutôt que strtol() ici : on fait confiance au FORMAT de
    // cette valeur, puisqu'elle vient de notre propre serveur qui
    // l'écrit lui-même avec snprintf("%zu", ...) — contrairement à une
    // entrée utilisateur, qu'il faudrait valider plus strictement.
    int archive_count = atoi(response_line);

    // Puis exactement 'archive_count' lignes, une par archive.
    for (int i = 0; i < archive_count; i++) {
        int name_length = receive_line(server_socket, response_line, sizeof(response_line));
        if (name_length < 0) {
            // Le serveur avait annoncé plus d'archives qu'il n'en a
            // finalement envoyé — connexion coupée en cours de route.
            fprintf(stderr, "Erreur reseau : connexion interrompue pendant la reception de la liste\n");
            return 1;
        }
        printf("%s\n", response_line);
    }

    return 0;
}