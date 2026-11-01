// ============================================================
//  Implémentation du mode LIST uniquement. N'utilise AUCUNE fonction
//  de archive.h : lister les archives disponibles sur le serveur est
//  une simple lecture de dossier (opendir/readdir), indépendante du
//  format interne d'une archive — on n'ouvre ni ne parse aucun fichier
//  .arch pour répondre à cette commande.
// ============================================================

#include "server_internal.h"
#include "protocol.h"
#include "network_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define ARCHIVE_FILE_EXTENSION ".arch"


static int filename_has_archive_extension(const char *filename)
{
    // On ne liste que les fichiers dont le nom se termine par ".arch" —
    // un dossier serveur pourrait contenir d'autres fichiers (logs,
    // README...) qu'il ne faut pas proposer comme archives.
    size_t filename_length = strlen(filename);
    size_t extension_length = strlen(ARCHIVE_FILE_EXTENSION);

    if (filename_length < extension_length) {
        // Un nom plus court que ".arch" lui-même ne peut évidemment pas
        // se terminer par ".arch" — évite une comparaison avec un index
        // négatif juste en dessous.
        return 0;
    }
    return strcmp(filename + (filename_length - extension_length), ARCHIVE_FILE_EXTENSION) == 0;
}


void handle_list_command(int client_socket, const char *archives_directory_path)
{
    DIR *directory = opendir(archives_directory_path);
    if (directory == NULL) {
        // Dossier introuvable ou droits insuffisants côté serveur —
        // erreur de CONFIGURATION du serveur, pas une erreur du client :
        // on répond ERR plutôt que de faire planter le serveur entier.
        send_line(client_socket, SERVER_RESPONSE_ERR);
        send_line(client_socket, "impossible d'ouvrir le dossier d'archives du serveur");
        return;
    }

    // On ne connaît pas à l'avance le nombre d'archives présentes :
    // tableau dynamique agrandi d'une case à chaque fichier ".arch"
    // trouvé, même logique de croissance que les tableaux de blocs dans
    // archive_raw_parser.c (le nombre d'archives sur un serveur de
    // projet reste de toute façon modeste).
    char **archive_names = NULL;
    size_t archive_count = 0;

    struct dirent *directory_entry;
    while ((directory_entry = readdir(directory)) != NULL) {
        if (!filename_has_archive_extension(directory_entry->d_name)) {
            continue;
        }

        char **grown_names = realloc(archive_names, (archive_count + 1) * sizeof(char *));
        if (grown_names == NULL) {
            // Mémoire épuisée en cours de route : on s'arrête là et on
            // répond quand même avec ce qu'on a déjà trouvé, plutôt que
            // de tout perdre pour un problème sur une seule entrée.
            break;
        }
        archive_names = grown_names;
        archive_names[archive_count] = strdup(directory_entry->d_name);
        if (archive_names[archive_count] != NULL) {
            archive_count++;
        }
    }
    closedir(directory);

    // Le protocole (protocol.h) demande : OK, puis le nombre
    // d'archives, puis une ligne par archive. On vérifie la réussite de
    // chaque envoi avant de continuer : si le client s'est déjà
    // déconnecté, inutile de s'acharner à envoyer la suite.
    if (send_line(client_socket, SERVER_RESPONSE_OK) == 0) {
        char count_as_text[32];
        snprintf(count_as_text, sizeof(count_as_text), "%zu", archive_count);
        if (send_line(client_socket, count_as_text) == 0) {
            for (size_t i = 0; i < archive_count; i++) {
                if (send_line(client_socket, archive_names[i]) < 0) {
                    break; // client déconnecté en cours d'envoi
                }
            }
        }
    }

    for (size_t i = 0; i < archive_count; i++) {
        free(archive_names[i]);
    }
    free(archive_names);
}