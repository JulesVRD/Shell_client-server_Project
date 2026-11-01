#ifndef NETWORK_IO_H
#define NETWORK_IO_H

#include <stddef.h>
// Fournit le type size_t, utilisé pour toutes les tailles/longueurs
// ci-dessous (voir explication détaillée donnée précédemment : size_t
// est non signé et sa taille s'adapte à l'architecture de la machine).

// ============================================================
//  Fonctions de transport réseau fiable, utilisées à la fois par vsh.c
//  (client) et server.c (serveur). TCP ne garantit de livrer les
//  octets que dans l'ordre, PAS en un seul bloc d'un coup : un seul
//  send()/recv() peut transférer moins que demandé. Ces fonctions
//  bouclent en interne pour garantir un transfert complet avant de
//  rendre la main à l'appelant.
// ============================================================

int send_all_bytes(int socket_file_descriptor,
                    const void *bytes_to_send,
                    size_t byte_count_to_send);
// Envoie EXACTEMENT 'byte_count_to_send' octets de 'bytes_to_send' sur
// le socket 'socket_file_descriptor', même si send() décide d'en
// envoyer moins d'un coup en interne (autorisé par POSIX, ça arrive en
// pratique sur un vrai réseau, pas seulement en théorie).
//
//   socket_file_descriptor : descripteur du socket TCP déjà connecté
//                            (obtenu via connect() côté client ou
//                            accept() côté serveur).
//   bytes_to_send          : adresse du premier octet à envoyer.
//                            Pointeur générique (void*) car cette
//                            fonction doit transporter aussi bien du
//                            texte (une commande) que des données
//                            binaires brutes (le contenu d'une
//                            archive) sans se soucier de leur type.
//   byte_count_to_send     : nombre exact d'octets à transmettre.
//
// Retourne 0 si tout a été envoyé avec succès, -1 en cas d'erreur
// réseau (y compris si le pair a fermé la connexion en cours d'envoi).

int receive_exact_bytes(int socket_file_descriptor,
                         void *received_bytes_buffer,
                         size_t byte_count_to_receive);
// Lit EXACTEMENT 'byte_count_to_receive' octets depuis
// 'socket_file_descriptor' et les place dans 'received_bytes_buffer'.
// Symétrique de send_all_bytes, pour les cas où la taille exacte à lire
// est déjà connue à l'avance (ex: le contenu complet d'une archive,
// dont la taille a été annoncée juste avant sur une ligne séparée).
//
//   received_bytes_buffer : zone mémoire déjà allouée par l'appelant,
//                            d'au moins 'byte_count_to_receive' octets
//                            de capacité. Pas de 'const' ici car cette
//                            fonction doit pouvoir ÉCRIRE dedans
//                            (contrairement à bytes_to_send ci-dessus,
//                            qui n'est que lu).
//
// Retourne 0 si succès, -1 si erreur réseau ou si la connexion s'est
// fermée avant d'avoir reçu tous les octets attendus.

int receive_line(int socket_file_descriptor,
                  char *received_line_buffer,
                  size_t received_line_buffer_capacity);
// Lit une ligne de commande terminée par '\n' depuis
// 'socket_file_descriptor' (ex: "LIST", "cd A", "OK") et la place dans
// 'received_line_buffer', toujours terminée par '\0'. Le caractère
// '\n' est consommé sur le socket mais jamais recopié dans le résultat.
// Contrairement à receive_exact_bytes, la longueur à lire n'est PAS
// connue à l'avance : elle dépend de l'endroit où arrive le '\n'.
//
//   received_line_buffer_capacity : taille totale du buffer fourni,
//                                   '\0' final inclus dans ce compte.
//
// Trois valeurs de retour possibles :
//   - un entier >= 0 : longueur de la ligne reçue (sans le '\n'), tout
//     s'est bien passé et la ligne tenait dans le buffer fourni.
//   - -1 : la connexion a été fermée par le pair sans qu'aucune ligne
//     complète n'ait pu être lue.
//   - -2 : la ligne reçue dépassait 'received_line_buffer_capacity'.
//     'received_line_buffer' contient alors une version TRONQUÉE de la
//     ligne, mais le flux réseau reste resynchronisé : tous les octets
//     en trop, jusqu'au '\n', ont bien été lus et jetés, donc l'appel
//     suivant repart correctement sur la ligne suivante.
//
// IMPORTANT : tout code appelant receive_line() doit traiter -1 et -2
// séparément, pas avec un simple "if (resultat < 0)" qui perdrait la
// distinction entre "connexion perdue" et "ligne tronquée mais
// connexion toujours utilisable".

int send_line(int socket_file_descriptor, const char *line_to_send);
// Envoie la chaîne 'line_to_send' suivie d'un '\n' sur
// 'socket_file_descriptor'. 'line_to_send' doit être une chaîne C
// classique terminée par '\0' (c'est l'appelant qui garantit ça, la
// fonction ne le vérifie pas).
//
// Retourne 0 si succès, -1 en cas d'erreur réseau.

#endif