// ============================================================
//  Réécriture COMPLÈTE du fichier .arch à partir de l'arbre ArchiveNode
//  actuel, après touch/mkdir/rm. Contrairement au parsing (qui lit une
//  fois), ici on régénère le header ET le body de zéro à chaque
//  sauvegarde — nécessaire car ajouter ou retirer un fichier change les
//  positions de TOUS les fichiers qui le suivaient dans le body, pas
//  seulement celui qu'on vient de modifier.
// ============================================================

#include "archive.h"
#include "archive_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// ============================================================
//  Petite chaîne de caractères EXTENSIBLE, privée à ce fichier : le
//  header et le body qu'on reconstruit n'ont pas de taille connue à
//  l'avance (ça dépend de tout l'arbre, contrairement à
//  read_file_content_from_archive où on connaissait déjà le nombre de
//  lignes) — il faut donc pouvoir grandir au fur et à mesure, comme
//  les tableaux de blocs/entrées dans archive_raw_parser.c.
// ============================================================

typedef struct {
    char *data;
    size_t length;      // nombre de caractères utiles actuellement stockés (hors '\0')
    size_t capacity;     // taille totale actuellement allouée pour data
} DynamicString;


static int dynamic_string_init(DynamicString *dynamic_string)
{
    dynamic_string->capacity = 256; // taille de départ arbitraire, raisonnable pour un petit header
    dynamic_string->data = malloc(dynamic_string->capacity);
    if (dynamic_string->data == NULL) {
        return -1;
    }
    dynamic_string->data[0] = '\0';
    dynamic_string->length = 0;
    return 0;
}


static int dynamic_string_append(DynamicString *dynamic_string, const char *text_to_append)
{
    size_t text_length = strlen(text_to_append);
    // +1 pour le '\0' final qu'on doit toujours pouvoir écrire après le
    // texte utile, même quand on est pile à la capacité.
    size_t needed_capacity = dynamic_string->length + text_length + 1;

    if (needed_capacity > dynamic_string->capacity) {
        // Doublement de la capacité (pas juste "+needed_capacity") :
        // stratégie de croissance classique pour un tableau dynamique —
        // amortit le coût des realloc sur le long terme, plutôt que de
        // ne réserver que l'espace juste nécessaire à chaque appel (ce
        // qui obligerait à réallouer à CHAQUE append, même pour une
        // chaîne courte).
        size_t new_capacity = dynamic_string->capacity;
        while (new_capacity < needed_capacity) {
            new_capacity *= 2;
        }
        char *grown_data = realloc(dynamic_string->data, new_capacity);
        if (grown_data == NULL) {
            return -1;
        }
        dynamic_string->data = grown_data;
        dynamic_string->capacity = new_capacity;
    }

    // memcpy de (text_length + 1) octets plutôt que strcat : on copie le
    // texte ET son '\0' final en une seule fois, à l'endroit exact où
    // s'arrête le contenu déjà présent — pas besoin que memcpy aille
    // elle-même rechercher la fin de dynamic_string->data à chaque appel
    // (ce que ferait strcat, un travail répété inutilement).
    memcpy(dynamic_string->data + dynamic_string->length, text_to_append, text_length + 1);
    dynamic_string->length += text_length;
    return 0;
}


static void dynamic_string_free(DynamicString *dynamic_string)
{
    free(dynamic_string->data);
    dynamic_string->data = NULL;
    dynamic_string->length = 0;
    dynamic_string->capacity = 0;
}


static long count_newlines_in_dynamic_string(const DynamicString *dynamic_string)
{
    long newline_count = 0;
    for (size_t i = 0; i < dynamic_string->length; i++) {
        if (dynamic_string->data[i] == '\n') {
            newline_count++;
        }
    }
    return newline_count;
}


static int serialize_directory_block(const Archive *archive, ArchiveNode *directory_node,
                                      const char *directory_path,
                                      DynamicString *header_buffer, DynamicString *body_buffer,
                                      long *body_line_counter)
{
    // Reproduit, à l'écriture, exactement la structure lue par
    // parse_raw_header_blocks à la lecture : une ligne "directory ...",
    // une ligne par entrée directe, puis "@". IMPORTANT : cette fonction
    // ne modifie AUCUN champ de l'arbre — elle ne fait que LIRE l'état
    // actuel et calculer les NOUVELLES valeurs qui seront écrites dans
    // le texte. Voir write_archive_to_file pour pourquoi cette
    // séparation lecture/écriture est délibérée.

    char line_buffer[1024];

    if (dynamic_string_append(header_buffer, "directory ") < 0) return -1;
    if (dynamic_string_append(header_buffer, directory_path) < 0) return -1;
    if (dynamic_string_append(header_buffer, "\n") < 0) return -1;

    for (size_t i = 0; i < directory_node->child_entry_count; i++) {
        ArchiveNode *child = directory_node->child_entries[i];

        if (child->entry_is_directory || child->entry_size_in_bytes == 0) {
            // Répertoire, OU fichier vide : 3 champs seulement, comme à
            // la lecture (voir le format du sujet, §1.1).
            snprintf(line_buffer, sizeof(line_buffer), "%s %s %ld\n",
                     child->entry_name, child->entry_permissions, child->entry_size_in_bytes);

        } else {
            // Fichier non vide : il faut (1) relire son contenu ACTUEL
            // pour pouvoir le recopier dans le nouveau body, en
            // utilisant ses ANCIENNES coordonnées (child->file_content_
            // start_line_in_body n'est PAS encore modifié à ce stade —
            // c'est précisément pour ça que l'ordre des opérations
            // ci-dessous compte), et (2) calculer sa NOUVELLE position
            // dans le body qu'on est en train de construire.
            char *existing_content = read_file_content_from_archive(archive, child);
            if (existing_content == NULL) {
                return -1;
            }

            long new_start_line = *body_line_counter + 1;
            snprintf(line_buffer, sizeof(line_buffer), "%s %s %ld %ld %ld\n",
                     child->entry_name, child->entry_permissions, child->entry_size_in_bytes,
                     new_start_line, child->file_content_line_count);

            if (dynamic_string_append(body_buffer, existing_content) < 0) {
                free(existing_content);
                return -1;
            }
            free(existing_content);
            if (dynamic_string_append(body_buffer, "\n") < 0) {
                return -1;
            }

            *body_line_counter += child->file_content_line_count;
        }

        if (dynamic_string_append(header_buffer, line_buffer) < 0) {
            return -1;
        }
    }

    if (dynamic_string_append(header_buffer, "@\n") < 0) {
        return -1;
    }

    // Les blocs des SOUS-répertoires viennent APRÈS avoir fini de
    // lister entièrement celui-ci — même convention que l'exemple du
    // sujet (le bloc racine entier d'abord, puis le bloc de A, puis
    // celui de A\A1...).
    for (size_t i = 0; i < directory_node->child_entry_count; i++) {
        ArchiveNode *child = directory_node->child_entries[i];
        if (child->entry_is_directory) {
            char *child_path = build_child_directory_path(directory_path, child->entry_name);
            if (child_path == NULL) {
                return -1;
            }
            int recursive_result = serialize_directory_block(
                archive, child, child_path, header_buffer, body_buffer, body_line_counter);
            free(child_path);
            if (recursive_result < 0) {
                return -1;
            }
        }
    }

    return 0;
}


static void update_body_positions_after_save(ArchiveNode *directory_node, long *body_line_counter)
{
    // Recalcule EXACTEMENT les mêmes nombres que serialize_directory_block
    // — ce qui exige IMPÉRATIVEMENT la même STRUCTURE de parcours, pas
    // seulement la même arithmétique : deux boucles séparées (entrées
    // d'abord, sous-répertoires ensuite), jamais entrelacées. Une
    // première version de ce fichier mélangeait les deux (récursion dès
    // qu'un sous-répertoire était rencontré, au milieu du traitement
    // des fichiers) : les deux fonctions calculaient alors des
    // positions DIFFÉRENTES pour les mêmes fichiers, ce qui corrompait
    // silencieusement l'archive dès la deuxième sauvegarde.

    // Boucle 1 : toutes les entrées DIRECTES de ce répertoire (fichiers
    // ET répertoires), dans l'ordre — seuls les fichiers non vides
    // avancent le compteur, mais on les visite TOUS dans cet ordre,
    // exactement comme le fait la boucle équivalente de
    // serialize_directory_block.
    for (size_t i = 0; i < directory_node->child_entry_count; i++) {
        ArchiveNode *child = directory_node->child_entries[i];
        if (!child->entry_is_directory && child->entry_size_in_bytes > 0) {
            child->file_content_start_line_in_body = *body_line_counter + 1;
            *body_line_counter += child->file_content_line_count;
        }
    }

    // Boucle 2 : SEULEMENT APRÈS avoir fini la boucle 1 en entier, on
    // descend dans les sous-répertoires — même ordre que la seconde
    // boucle de serialize_directory_block, jamais entrelacée avec la
    // première.
    for (size_t i = 0; i < directory_node->child_entry_count; i++) {
        ArchiveNode *child = directory_node->child_entries[i];
        if (child->entry_is_directory) {
            update_body_positions_after_save(child, body_line_counter);
        }
    }
}


int write_archive_to_file(Archive *archive)
{
    DynamicString header_buffer;
    DynamicString body_buffer;

    if (dynamic_string_init(&header_buffer) < 0) {
        return -1;
    }
    if (dynamic_string_init(&body_buffer) < 0) {
        dynamic_string_free(&header_buffer);
        return -1;
    }

    long body_line_counter = 0;

    // ÉTAPE 1 (lecture seule) : construit le texte du header et du body
    // dans des buffers en mémoire, SANS modifier le moindre champ de
    // l'arbre. root_entry->entry_name contient le chemin D'ORIGINE du
    // bloc racine (ex: "Exemple\Test\"), conservé tel quel depuis le
    // premier archive_parse() — c'est CE chemin, pas "\", qui doit
    // réapparaître littéralement dans le fichier .arch réécrit. Si quoi
    // que ce soit échoue plus bas (écriture disque), l'arbre en mémoire
    // doit rester dans son état D'AVANT la tentative de sauvegarde,
    // cohérent avec le fichier sur le disque qui, lui, n'aura pas bougé
    // non plus.
    int serialize_result = serialize_directory_block(
        archive, archive->root_entry, archive->root_entry->entry_name,
        &header_buffer, &body_buffer, &body_line_counter);

    if (serialize_result < 0) {
        dynamic_string_free(&header_buffer);
        dynamic_string_free(&body_buffer);
        return -1;
    }

    long header_line_count = count_newlines_in_dynamic_string(&header_buffer);
    long new_header_start_line = 2;
    // Convention assumée : le header commence toujours directement à la
    // ligne 2, sans ligne vide intercalée (contrairement à l'exemple du
    // sujet, qui en avait une à la ligne 2 — rien dans le format ne
    // l'impose, archive_raw_parser.c gère de toute façon n'importe quel
    // nombre de lignes sautées entre la ligne 1 et le header).
    long new_body_start_line = new_header_start_line + header_line_count;

    // ÉTAPE 2 : écriture dans un fichier TEMPORAIRE. Si l'écriture
    // s'interrompait en cours de route (disque plein, perte
    // d'alimentation...), écrire directement dans le fichier d'origine
    // le laisserait à moitié tronqué, donc définitivement corrompu — la
    // seule copie de cette archive serait perdue. En passant par un
    // fichier ".tmp" puis un rename() final (qui est une opération
    // ATOMIQUE sur un système de fichiers POSIX), soit toute l'opération
    // réussit, soit le fichier d'origine n'est jamais touché.
    char temporary_file_path[2048];
    snprintf(temporary_file_path, sizeof(temporary_file_path), "%s.tmp", archive->source_file_path);

    FILE *output_file = fopen(temporary_file_path, "w");
    if (output_file == NULL) {
        dynamic_string_free(&header_buffer);
        dynamic_string_free(&body_buffer);
        return -1;
    }

    fprintf(output_file, "%ld:%ld\n", new_header_start_line, new_body_start_line);
    fputs(header_buffer.data, output_file);
    fputs(body_buffer.data, output_file);

    // ferror() AVANT fclose() : fclose() peut elle-même déclencher une
    // dernière écriture (vidage du tampon interne de la bibliothèque C)
    // qui pourrait échouer silencieusement si on ne vérifie pas l'état
    // du flux juste avant de le fermer.
    int write_encountered_error = ferror(output_file);
    fclose(output_file);

    dynamic_string_free(&header_buffer);
    dynamic_string_free(&body_buffer);

    if (write_encountered_error) {
        remove(temporary_file_path);
        return -1;
    }

    // ÉTAPE 3 : bascule atomique vers le fichier final.
    if (rename(temporary_file_path, archive->source_file_path) != 0) {
        remove(temporary_file_path);
        return -1;
    }

    // ÉTAPE 4 : mise à jour de l'arbre EN MÉMOIRE, maintenant que le
    // fichier sur le disque correspond réellement à ces valeurs.
    archive->body_start_line_in_file = new_body_start_line;
    long update_counter = 0;
    update_body_positions_after_save(archive->root_entry, &update_counter);

    return 0;
}