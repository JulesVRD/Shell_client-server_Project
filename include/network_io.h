#ifndef NETWORK_IO_H
#define NETWORK_IO_H

#include <stddef.h>

// Envoie EXACTEMENT 'length' octets de 'data' sur le socket 'socket_fd',
// même si send() décide d'en envoyer moins d'un coup en interne (c'est
// autorisé par POSIX, et ça arrive en pratique sur un vrai réseau).
// Retourne 0 si tout a été envoyé avec succès, -1 en cas d'erreur
// (y compris si le pair a fermé la connexion en cours d'envoi).
int send_all_bytes(int socket_fd, const void *data, size_t length);

// Lit EXACTEMENT 'byte_count' octets depuis 'socket_fd' et les place
// dans 'buffer' (qui doit déjà avoir au moins 'byte_count' octets de
// place réservée). Même logique que send_all_bytes mais en lecture.
// Retourne 0 si succès, -1 si erreur ou si la connexion s'est fermée
// avant d'avoir reçu tous les octets attendus.
int receive_exact_bytes(int socket_fd, void *buffer, size_t byte_count);

// Lit une ligne terminée par '\n' depuis 'socket_fd' et la place dans
// 'line_buffer' (toujours terminée par '\0'). Le '\n' n'est jamais
// inclus dans le résultat.
//   - Retourne la longueur de la ligne (sans le \n) si tout s'est bien
//     passé et que la ligne tenait dans 'buffer_capacity' octets.
//   - Retourne -1 si la connexion a été fermée sans qu'aucune ligne
//     n'ait pu être lue.
//   - Retourne -2 si la ligne reçue dépassait 'buffer_capacity' - 1
//     octets : 'line_buffer' contient alors une version TRONQUÉE de la
//     ligne, mais le flux réseau reste resynchronisé (les octets en
//     trop, jusqu'au '\n', ont bien été consommés du socket).
int receive_line(int socket_fd, char *line_buffer, size_t buffer_capacity);

// Envoie la chaîne 'line' suivie d'un '\n' sur 'socket_fd'.
int send_line(int socket_fd, const char *line);

#endif