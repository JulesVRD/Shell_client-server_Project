#include "network_io.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int send_all_bytes(int socket_fd, const void *data, size_t length)
{
    const char *remaining_data = data;
    size_t bytes_sent_so_far = 0;

    while (bytes_sent_so_far < length) {
        // MSG_NOSIGNAL : si le pair a déjà fermé la connexion, send()
        // déclenche normalement le signal SIGPIPE, dont l'action par
        // défaut est de TUER tout le processus (client ou serveur) sans
        // même laisser cette fonction retourner -1 proprement. Ce flag
        // désactive ce signal pour cet appel : on récupère l'erreur
        // normalement via la valeur de retour (errno == EPIPE), gérée
        // juste en dessous. Sans ça, un client qui ferme sa connexion
        // en plein envoi pourrait planter tout le serveur.
        ssize_t bytes_sent_now = send(socket_fd,
                                       remaining_data + bytes_sent_so_far,
                                       length - bytes_sent_so_far,
                                       MSG_NOSIGNAL);
        if (bytes_sent_now < 0) {
            if (errno == EINTR) continue;   // interrompu par un signal : on retente
            return -1;                       // vraie erreur réseau (ex: EPIPE)
        }
        if (bytes_sent_now == 0) return -1;  // ne devrait pas arriver avec send()
        bytes_sent_so_far += (size_t)bytes_sent_now;
    }
    return 0;
}

int receive_exact_bytes(int socket_fd, void *buffer, size_t byte_count)
{
    char *write_position = buffer;
    size_t bytes_received_so_far = 0;

    while (bytes_received_so_far < byte_count) {
        ssize_t bytes_received_now = recv(socket_fd,
                                           write_position + bytes_received_so_far,
                                           byte_count - bytes_received_so_far,
                                           0);
        if (bytes_received_now < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (bytes_received_now == 0) return -1;  // le pair a fermé la connexion trop tôt
        bytes_received_so_far += (size_t)bytes_received_now;
    }
    return 0;
}

int receive_line(int socket_fd, char *line_buffer, size_t buffer_capacity)
{
    // Garde-fou : avec buffer_capacity == 0, 'buffer_capacity - 1' plus
    // bas déborderait silencieusement (buffer_capacity est un size_t,
    // donc non signé : 0 - 1 ne vaut pas -1 mais l'immense valeur
    // maximale représentable). On refuse ce cas dès l'entrée plutôt que
    // de laisser la fonction écrire hors des limites du buffer.
    if (buffer_capacity == 0) return -1;

    size_t chars_written = 0;
    int line_too_long = 0;   // passe à 1 si la ligne dépasse la capacité du buffer

    while (1) {
        char current_char;
        ssize_t result = recv(socket_fd, &current_char, 1, 0);
        if (result < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (result == 0) {
            // Connexion fermée par le pair avant d'avoir reçu de '\n'.
            // Si on n'a reçu AUCUN caractère, rien à retourner : vraie
            // erreur. Sinon, on retourne quand même ce qu'on a (ligne
            // non terminée par un '\n', comportement conservé tel quel).
            if (chars_written == 0 && !line_too_long) return -1;
            break;
        }
        if (current_char == '\n') break;

        if (chars_written < buffer_capacity - 1) {
            line_buffer[chars_written++] = current_char;
        } else {
            // Buffer plein : on NE s'arrête PAS ici. On continue à lire
            // (sans plus rien stocker) jusqu'au prochain '\n', pour vider
            // la fin de la ligne du socket. C'est la correction du bug
            // identifié plus tôt : sans ça, les octets en trop restent en
            // attente dans le flux et désynchronisent toutes les lectures
            // suivantes.
            line_too_long = 1;
        }
    }

    line_buffer[chars_written] = '\0';
    if (line_too_long) return -2;   // ligne tronquée, mais flux resynchronisé
    return (int)chars_written;
}

int send_line(int socket_fd, const char *line)
{
    size_t line_length = strlen(line);
    if (send_all_bytes(socket_fd, line, line_length) < 0) return -1;
    return send_all_bytes(socket_fd, "\n", 1);
}