#ifndef ARCHIVE_INTERNAL_H
#define ARCHIVE_INTERNAL_H

#include "archive.h"
#include <stddef.h>
#include <stdio.h>
// <stdio.h> pour le type FILE, utilisé par read_one_line_from_archive_file
// ci-dessous.

// ============================================================
//  Déclarations PRIVÉES, partagées uniquement entre les fichiers qui
//  implémentent le module "archive" en interne : ce header lui-même,
//  archive_raw_parser.c, archive_tree_builder.c, archive_navigation.c,
//  archive_body.c et archive_persistence.c. Volontairement PAS dans
//  include/ : vsh.c et server.c n'ont jamais besoin de connaître
//  RawHeaderEntry, RawHeaderBlock, ni les fonctions ci-dessous — seul
//  ce qui est déclaré dans archive.h forme le vrai contrat public de
//  ce module.
// ============================================================

typedef struct {
    char *raw_entry_name;
    char raw_entry_permissions[11];
    long raw_entry_size_in_bytes;
    long raw_entry_file_content_start_line_in_body;
    long raw_entry_file_content_line_count;
} RawHeaderEntry;
// Une entrée telle que lue TEXTUELLEMENT dans un bloc du header, avant
// toute mise en relation parent/enfant. Voir archive_raw_parser.c.

typedef struct {
    char *raw_block_directory_path;
    RawHeaderEntry *raw_block_entries;
    size_t raw_block_entry_count;
} RawHeaderBlock;
// Un bloc "directory ... / entrées / @" tel que lu dans le fichier,
// identifié par son chemin complet. Voir archive_raw_parser.c.


// --- Fournies par archive_raw_parser.c, utilisées par archive.c -----

RawHeaderBlock *parse_raw_header_blocks_from_archive_file(
    const char *archive_file_path,
    long *out_body_start_line_number,
    size_t *out_block_count);
// Point d'entrée de la passe 1 : ouvre le fichier, lit la ligne 1,
// avance jusqu'au header, puis lit tous les blocs jusqu'à
// body_start_line_number. Retourne un tableau alloué (à libérer avec
// free_raw_header_blocks), ou NULL si l'archive est introuvable ou
// mal formée.

void free_raw_header_blocks(RawHeaderBlock *header_blocks_array,
                             size_t header_blocks_array_count);
// Libère tout ce qu'a alloué la passe 1 (noms, tableaux d'entrées,
// chemins de blocs, tableau de blocs lui-même).


// --- Fournies par archive_tree_builder.c, utilisées par archive.c ---

int build_archive_subtree_from_raw_blocks(const RawHeaderBlock *raw_blocks_array,
                                           size_t raw_blocks_array_count,
                                           const char *current_directory_path,
                                           ArchiveNode *current_directory_node);
// Point d'entrée de la passe 2 : peuple récursivement
// current_directory_node->child_entries à partir des blocs bruts de la
// passe 1, en retrouvant chaque sous-répertoire par son chemin complet.
// Retourne 0 si succès, -1 si un sous-répertoire annoncé n'a pas de
// bloc correspondant (archive corrompue).

void free_archive_node_recursive(ArchiveNode *node_to_free);
// Libère un ArchiveNode et tout son sous-arbre (enfants avant parent).


// --- Fournie par archive_navigation.c, utilisée par archive_tree_builder.c ---

char *build_child_directory_path(const char *parent_directory_path,
                                  const char *child_directory_name);
// Concatène un chemin et un nom pour obtenir le chemin de l'enfant,
// en gérant le cas particulier de la racine (voir archive_navigation.c
// pour le détail). Utilisée dans les DEUX sens : par la passe 2 pour
// descendre vers les feuilles, et par build_absolute_path_string pour
// remonter vers la racine (pwd) — d'où sa présence ici plutôt que dans
// l'un ou l'autre fichier spécifiquement.


// --- Fournie par archive_raw_parser.c, utilisée par archive_body.c
//     et archive_persistence.c ---

int read_one_line_from_archive_file(FILE *archive_file,
                                     char *line_content_buffer,
                                     size_t line_content_buffer_capacity);
// Lit une ligne d'un fichier .arch déjà ouvert (voir archive_raw_parser.c
// pour le détail de la gestion du '\n' et des lignes trop longues).
// Rendue non-static car archive_body.c en a besoin pour se positionner
// dans le body, et archive_persistence.c en aura besoin pour relire
// l'ancien contenu avant de le réécrire.

#endif