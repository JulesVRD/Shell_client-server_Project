// ============================================================
//  Ce fichier NE CONTIENT QUE le squelette réseau (création du socket,
//  boucle d'acceptation des clients) et l'AIGUILLAGE vers la bonne
//  fonction selon la commande reçue. Toute la logique propre à chaque
//  commande vit dans son propre fichier : server_list.c pour LIST,
//  puis server_browse.c, server_create.c, server_extract.c à venir —
//  déclarées dans server_internal.h, jamais implémentées ici.
// ============================================================

#include "server_internal.h"
#include "protocol.h"
#include "network_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define LISTEN_BACKLOG 10
// Nombre maximal de connexions clientes en attente d'être acceptées
// (accept()) avant que le système ne les refuse. 10 est une valeur
// conventionnelle, largement suffisante vu qu'on traite les clients un
// par un pour l'instant (voir la boucle principale dans main()).


static int create_listening_socket(int port_number)
{
    int listening_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_socket < 0) {
        perror("socket");
        return -1;
    }

    // SO_REUSEADDR : sans cette option, relancer le serveur juste après
    // l'avoir arrêté échoue souvent avec "Address already in use", car
    // le système garde le port réservé un moment après la fermeture
    // (état TIME_WAIT de TCP). Purement un confort de développement/
    // test, sans rapport avec le protocole applicatif lui-même.
    int reuse_address_option = 1;
    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR,
               &reuse_address_option, sizeof(reuse_address_option));

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY; // écoute sur toutes les interfaces réseau de la machine
    // htons : convertit le numéro de port de l'ordre des octets de LA
    // MACHINE vers l'ordre "réseau" (big-endian), imposé par TCP/IP
    // indépendamment de l'architecture sur laquelle le serveur tourne.
    server_address.sin_port = htons((uint16_t)port_number);

    if (bind(listening_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        close(listening_socket);
        return -1;
    }

    if (listen(listening_socket, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(listening_socket);
        return -1;
    }

    return listening_socket;
}


int main(int argument_count, char *argument_values[])
{
    if (argument_count != 3) {
        fprintf(stderr, "Usage: %s <port> <dossier_archives>\n", argument_values[0]);
        return 1;
    }

    int port_number = atoi(argument_values[1]);
    const char *archives_directory_path = argument_values[2];

    int listening_socket = create_listening_socket(port_number);
    if (listening_socket < 0) {
        return 1;
    }

    printf("Serveur en ecoute sur le port %d, archives dans '%s'\n",
           port_number, archives_directory_path);
    fflush(stdout);
    // fflush explicite : stdout est mis en mémoire tampon par bloc (pas
    // ligne par ligne) quand la sortie ne va pas vers un terminal — sans
    // ça, ce message pourrait rester invisible un moment si on lance le
    // serveur en arrière-plan ou qu'on redirige sa sortie vers un fichier.

    // Boucle principale : accepte UN client à la fois, le traite
    // entièrement, puis passe au suivant. Volontairement séquentiel
    // pour l'instant — gérer plusieurs clients EN MÊME TEMPS (le mode
    // browse en particulier, où une session reste ouverte un moment)
    // demandera fork()/threads/select() plus tard ; pas nécessaire tant
    // qu'on teste un seul client à la fois.
    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);

        int client_socket = accept(listening_socket,
                                    (struct sockaddr *)&client_address,
                                    &client_address_length);
        if (client_socket < 0) {
            perror("accept");
            continue; // une erreur ponctuelle sur un client ne doit pas arrêter tout le serveur
        }

        char command_line[MAX_PROTOCOL_LINE_LENGTH];
        int command_length = receive_line(client_socket, command_line, sizeof(command_line));

        // --- Aiguillage : UN SEUL endroit du programme qui décide quelle
        //     fonction appeler selon la commande reçue. Ajouter un futur
        //     mode (CREATE/BROWSE/EXTRACT) ne touchera qu'à ce bloc et à
        //     son propre fichier server_xxx.c, jamais à server_list.c ni
        //     à la boucle réseau elle-même.
        if (command_length >= 0 && strcmp(command_line, CLIENT_COMMAND_LIST) == 0) {
            handle_list_command(client_socket, archives_directory_path);
        } else if (command_length >= 0) {
            // Une ligne a bien été reçue, mais aucun mode implémenté ne
            // correspond pour l'instant (CREATE/BROWSE/EXTRACT viendront
            // plus tard).
            send_line(client_socket, SERVER_RESPONSE_ERR);
            send_line(client_socket, "commande non reconnue ou pas encore implementee");
        }
        // command_length < 0 (connexion fermée avant d'envoyer quoi que
        // ce soit, ou ligne trop longue) : rien à répondre, le client
        // est de toute façon probablement déjà parti.

        close(client_socket);
    }

    // Jamais atteint (boucle infinie ci-dessus), gardé pour la symétrie
    // et pour le jour où un arrêt propre (ex: signal SIGINT) sera géré.
    close(listening_socket);
    return 0;
}