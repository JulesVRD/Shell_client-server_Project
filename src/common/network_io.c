#include "network_io.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int send_all_bytes(int socket_file_descriptor,
                    const void *bytes_to_send,
                    size_t byte_count_to_send)
{
    // bytes_to_send arrive en void* (type générique, voir network_io.h),
    // ce qui interdit l'arithmétique de pointeur. On le convertit donc
    // en char* : sizeof(char) vaut 1 octet par définition du langage,
    // donc avancer ce pointeur d'une unité avance exactement d'un
    // octet, ce qui est ce qu'il faut pour compter des octets envoyés.
    const char *bytes_to_send_cursor = bytes_to_send;
    size_t total_bytes_sent_so_far = 0;

    while (total_bytes_sent_so_far < byte_count_to_send) {
        // MSG_NOSIGNAL : si le pair a déjà fermé la connexion, send()
        // déclenche normalement le signal SIGPIPE, dont l'action par
        // défaut est de TUER tout le processus (client ou serveur)
        // sans même laisser cette fonction retourner -1 proprement.
        // Ce flag désactive ce signal pour cet appel : l'erreur remonte
        // normalement via la valeur de retour (errno == EPIPE), gérée
        // juste en dessous. Sans ça, un seul client qui ferme sa
        // connexion en plein envoi pourrait planter tout le serveur,
        // y compris les autres clients connectés en même temps.
        ssize_t bytes_sent_in_this_call =
            send(socket_file_descriptor,
                 bytes_to_send_cursor + total_bytes_sent_so_far,
                 byte_count_to_send - total_bytes_sent_so_far,
                 MSG_NOSIGNAL);

        if (bytes_sent_in_this_call < 0) {
            if (errno == EINTR) continue;   // interrompu par un signal : on retente
            return -1;                       // vraie erreur réseau (ex: EPIPE)
        }
        if (bytes_sent_in_this_call == 0) return -1;  // ne devrait pas arriver avec send()

        // Conversion explicite (size_t) : bytes_sent_in_this_call est un
        // ssize_t (signé, pour pouvoir représenter -1 en cas d'erreur),
        // mais on vient de vérifier qu'il n'est pas négatif, donc on
        // peut l'additionner sans risque à total_bytes_sent_so_far (qui
        // est un size_t, non signé) — le cast documente l'intention et
        // évite un avertissement du compilateur sur ce mélange de types.
        total_bytes_sent_so_far += (size_t)bytes_sent_in_this_call;
    }
    return 0;
}

int receive_exact_bytes(int socket_file_descriptor,
                         void *received_bytes_buffer,
                         size_t byte_count_to_receive)
{
    char *received_bytes_cursor = received_bytes_buffer;
    size_t total_bytes_received_so_far = 0;

    while (total_bytes_received_so_far < byte_count_to_receive) {
        ssize_t bytes_received_in_this_call =
            recv(socket_file_descriptor,
                 received_bytes_cursor + total_bytes_received_so_far,
                 byte_count_to_receive - total_bytes_received_so_far,
                 0);

        if (bytes_received_in_this_call < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        // Pour recv() (contrairement à send()), un retour de 0 a une
        // signification précise : le pair a fermé proprement sa
        // connexion (TCP close). Si ça arrive avant d'avoir reçu tous
        // les octets attendus, on sait qu'on ne les recevra jamais.
        if (bytes_received_in_this_call == 0) return -1;

        total_bytes_received_so_far += (size_t)bytes_received_in_this_call;
    }
    return 0;
}

int receive_line(int socket_file_descriptor,
                  char *received_line_buffer,
                  size_t received_line_buffer_capacity)
{
    // Garde-fou : avec received_line_buffer_capacity == 0, l'expression
    // "capacity - 1" plus bas déborderait silencieusement (c'est un
    // size_t, donc non signé : 0 - 1 ne vaut pas -1 mais l'immense
    // valeur maximale représentable). On refuse ce cas dès l'entrée.
    if (received_line_buffer_capacity == 0) return -1;

    size_t total_characters_written = 0;
    int line_was_truncated = 0;   // passe à 1 si la ligne dépasse la capacité du buffer

    while (1) {
        char current_character;
        ssize_t read_result = recv(socket_file_descriptor, &current_character, 1, 0);

        if (read_result < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (read_result == 0) {
            // Connexion fermée par le pair avant d'avoir reçu de '\n'.
            // Si on n'a reçu AUCUN caractère, rien à retourner : vraie
            // erreur. Sinon, on retourne quand même ce qu'on a (ligne
            // non terminée par un '\n', comportement conservé tel quel).
            if (total_characters_written == 0 && !line_was_truncated) return -1;
            break;
        }
        if (current_character == '\n') break;

        if (total_characters_written < received_line_buffer_capacity - 1) {
            received_line_buffer[total_characters_written++] = current_character;
        } else {
            // Buffer plein : on NE s'arrête PAS ici. On continue à lire
            // (sans plus rien stocker) jusqu'au prochain '\n', pour
            // vider la fin de la ligne du socket. Sans ça, les octets en
            // trop resteraient en attente dans le flux et
            // désynchroniseraient toutes les lectures suivantes sur
            // cette connexion.
            line_was_truncated = 1;
        }
    }

    received_line_buffer[total_characters_written] = '\0';
    if (line_was_truncated) return -2;   // ligne tronquée, mais flux resynchronisé
    return (int)total_characters_written;
}

int send_line(int socket_file_descriptor, const char *line_to_send)
{
    size_t line_length_in_bytes = strlen(line_to_send);
    if (send_all_bytes(socket_file_descriptor, line_to_send, line_length_in_bytes) < 0) {
        return -1;
    }
    return send_all_bytes(socket_file_descriptor, "\n", 1);
}