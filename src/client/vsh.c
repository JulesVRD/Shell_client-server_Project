// ============================================================
//  Ce fichier NE CONTIENT QUE le parsing de la ligne de commande, la
//  connexion au serveur, et l'AIGUILLAGE vers la bonne fonction selon
//  le mode demandé (-list/-create/-browse/-extract). Toute la logique
//  propre à chaque mode vit dans son propre fichier : client_list.c
//  pour -list, puis client_create.c, client_browse.c, client_extract.c
//  à venir — déclarées dans client_internal.h, jamais implémentées ici.
//  Symétrique de server.c côté serveur.
// ============================================================

#define _POSIX_C_SOURCE 200809L //evite à addrinfo de renvoyer des erreru de imcomplete type

#include "client_internal.h"
#include "protocol.h"
#include "network_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>


static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s -list nom_serveur port\n", program_name);
    fprintf(stderr, "  %s -create nom_serveur port nom_archive\n", program_name);
    fprintf(stderr, "  %s -browse nom_serveur port nom_archive\n", program_name);
    fprintf(stderr, "  %s -extract nom_serveur port nom_archive\n", program_name);
}


static int create_connected_socket(const char *server_name, const char *port_as_text)
{
    // getaddrinfo() plutôt que inet_pton()/inet_addr() : ces dernières
    // n'acceptent qu'une adresse IP littérale ("192.168.1.10"), alors
    // que le sujet parle d'un "nom_serveur" — qui peut tout à fait être
    // un nom de machine ("localhost", "archives.local"...) à résoudre
    // en adresse IP. getaddrinfo() gère les deux cas de façon uniforme,
    // et accepte aussi bien l'IPv4 que l'IPv6 sans code séparé pour
    // chaque cas (AF_UNSPEC ci-dessous : "peu importe lequel des deux").

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; // TCP, pas UDP (SOCK_DGRAM)

    // port_as_text est passé TEL QUEL (la chaîne reçue sur la ligne de
    // commande, ex: "9000") : getaddrinfo() accepte un port sous forme
    // de texte directement — pas besoin de le convertir en entier pour
    // le reconvertir ensuite en réseau (htons) comme côté serveur, cette
    // étape est entièrement prise en charge ici.
    struct addrinfo *resolved_addresses;
    int getaddrinfo_result = getaddrinfo(server_name, port_as_text, &hints, &resolved_addresses);
    if (getaddrinfo_result != 0) {
        fprintf(stderr, "Impossible de resoudre '%s' : %s\n",
                server_name, gai_strerror(getaddrinfo_result));
        return -1;
    }

    // Un nom peut résoudre vers PLUSIEURS adresses (plusieurs IP pour un
    // même nom de domaine, IPv4 et IPv6 mélangés...) : on essaie chacune
    // à son tour jusqu'à ce qu'une connexion aboutisse, plutôt que de
    // s'arrêter net sur la première qui échoue.
    int connected_socket = -1;
    for (struct addrinfo *candidate = resolved_addresses; candidate != NULL; candidate = candidate->ai_next) {
        int socket_fd = socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (socket_fd < 0) {
            continue; // cette combinaison de famille/protocole n'est pas supportée ici, on tente la suivante
        }
        if (connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen) == 0) {
            connected_socket = socket_fd;
            break;
        }
        close(socket_fd); // échec de connexion sur CETTE adresse : on ferme avant d'essayer la suivante
    }

    // freeaddrinfo() libère la liste chaînée allouée par getaddrinfo() —
    // à faire après la boucle, qu'une connexion ait réussi ou non,
    // puisqu'on n'a plus besoin des adresses résolues dans les deux cas.
    freeaddrinfo(resolved_addresses);

    if (connected_socket < 0) {
        fprintf(stderr, "Impossible de se connecter a '%s' sur le port %s\n", server_name, port_as_text);
    }
    return connected_socket;
}


int main(int argument_count, char *argument_values[])
{
    // -list demande 4 arguments (programme, mode, serveur, port) ; les
    // trois autres modes en demandent 5 (+ nom_archive) — on vérifie
    // donc le minimum ici, et le mode exact plus bas avant de dispatcher.
    if (argument_count < 4) {
        print_usage(argument_values[0]);
        return 1;
    }

    const char *mode = argument_values[1];
    const char *server_name = argument_values[2];
    const char *port_as_text = argument_values[3];

    int server_socket = create_connected_socket(server_name, port_as_text);
    if (server_socket < 0) {
        return 1;
    }

    // --- Aiguillage : UN SEUL endroit du programme qui décide quelle
    //     fonction appeler selon le mode demandé — même principe que
    //     l'aiguillage dans server.c. Ajouter un futur mode ne touchera
    //     qu'à ce bloc et à son propre fichier client_xxx.c.
    int command_result;
    if (strcmp(mode, "-list") == 0) {
        command_result = run_list_command(server_socket);

    } else if (strcmp(mode, "-create") == 0 || strcmp(mode, "-browse") == 0 || strcmp(mode, "-extract") == 0) {
        // Modes reconnus par le sujet, mais pas encore implémentés dans
        // ce projet — message clair plutôt qu'un silence ou un crash.
        fprintf(stderr, "Mode %s : pas encore implemente\n", mode);
        command_result = 1;

    } else {
        fprintf(stderr, "Mode inconnu : %s\n", mode);
        print_usage(argument_values[0]);
        command_result = 1;
    }

    close(server_socket);
    return command_result;
}