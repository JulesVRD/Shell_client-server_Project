// ============================================================
//  Ce fichier NE CONTIENT QUE l'orchestration de archive_parse et
//  archive_free. Tout le détail d'implémentation vit ailleurs dans
//  common/ : archive_raw_parser.c (passe 1), archive_tree_builder.c
//  (passe 2), archive_navigation.c (pwd/ls/cd), archive_body.c
//  (lecture du contenu d'un fichier), archive_persistence.c
//  (réécriture sur le disque), archive_mutation.c (touch/mkdir/rm).
//  server.c et vsh.c n'ont besoin de connaître aucun de ces fichiers
//  directement : tout passe par ce qui est déclaré dans archive.h.
// ============================================================

#include "archive.h"
#include "archive_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Archive *archive_parse(const char *archive_file_path)
{
    // --- Passe 1 : lecture brute du header (voir archive_raw_parser.c) ---
    long body_start_line_number = 0;
    size_t raw_block_count = 0;
    RawHeaderBlock *raw_blocks_array = parse_raw_header_blocks_from_archive_file(
        archive_file_path, &body_start_line_number, &raw_block_count);

    if (raw_blocks_array == NULL || raw_block_count == 0) {
        // raw_block_count == 0 est un cas à part : le fichier était
        // valide (sinon raw_blocks_array vaudrait déjà NULL) mais ne
        // contenait AUCUN bloc "directory", donc aucune racine
        // exploitable. free_raw_header_blocks(NULL, 0) ne fait rien de
        // dangereux (boucle de 0 itérations, free(NULL) est un no-op en
        // C) : l'appeler systématiquement ici évite un if séparé pour ce
        // cas.
        free_raw_header_blocks(raw_blocks_array, raw_block_count);
        return NULL;
    }

    // --- Allocation de la structure Archive à retourner ---
    Archive *new_archive = malloc(sizeof(Archive));
    if (new_archive == NULL) {
        free_raw_header_blocks(raw_blocks_array, raw_block_count);
        return NULL;
    }

    // --- Création du nœud racine, cas particulier non couvert par
    //     build_archive_subtree_from_raw_blocks ---
    ArchiveNode *root_node = malloc(sizeof(ArchiveNode));
    if (root_node == NULL) {
        free(new_archive);
        free_raw_header_blocks(raw_blocks_array, raw_block_count);
        return NULL;
    }

    // raw_blocks_array[0] est TOUJOURS le bloc racine : c'est le tout
    // premier bloc "directory" rencontré en lisant le header de haut en
    // bas (voir parse_raw_header_blocks), et c'est forcément celui qui
    // décrit le répertoire donné au mode -create à l'origine.
    root_node->entry_name = strdup(raw_blocks_array[0].raw_block_directory_path);
    if (root_node->entry_name == NULL) {
        // root_node lui-même a été alloué juste au-dessus avec succès,
        // mais pas son nom : il faut le libérer explicitement avant de
        // partir en erreur, sinon il fuit (il n'est encore rattaché à
        // aucune structure Archive terminée).
        free(root_node);
        free(new_archive);
        free_raw_header_blocks(raw_blocks_array, raw_block_count);
        return NULL;
    }
    root_node->entry_is_directory = 1;
    // Contrairement à tous les autres ArchiveNode (créés à partir d'une
    // LIGNE D'ENTRÉE dans le bloc de leur parent, avec de vrais droits
    // d'accès lus dans l'archive), la racine n'est annoncée par AUCUNE
    // entrée — c'est le point de départ de toute la récursion, pas le
    // résultat d'un parsing de ligne. Le format du sujet n'écrit jamais
    // ses droits d'accès propres nulle part : ce placeholder neutre est
    // donc un choix assumé, pas une vraie valeur lue depuis l'archive —
    // à revoir si "ls -l ." sur la racine elle-même devient nécessaire.
    strcpy(root_node->entry_permissions, "d---------");
    root_node->entry_size_in_bytes = 0;
    root_node->file_content_start_line_in_body = 0;
    root_node->file_content_line_count = 0;
    root_node->child_entries = NULL;
    root_node->child_entry_count = 0;
    // NULL : la racine est le seul ArchiveNode de tout l'arbre à ne
    // jamais avoir de parent — c'est précisément ce qui permettra à "cd .."
    // de détecter, plus tard, qu'on ne peut pas remonter plus haut.
    root_node->parent_directory = NULL;

    // strdup et non une simple affectation de pointeur : archive_file_path
    // appartient à l'APPELANT de archive_parse (souvent une chaîne
    // temporaire construite pour cet appel précis) — Archive a besoin de
    // SA PROPRE copie, qui doit rester valide bien après le retour de
    // cette fonction, pendant toute la durée de la session de browse.
    new_archive->source_file_path = strdup(archive_file_path);
    if (new_archive->source_file_path == NULL) {
        free(root_node->entry_name);
        free(root_node);
        free(new_archive);
        free_raw_header_blocks(raw_blocks_array, raw_block_count);
        return NULL;
    }

    new_archive->root_entry = root_node;
    new_archive->body_start_line_in_file = body_start_line_number;

    // --- Passe 2 : construction du reste de l'arbre (voir archive_tree_builder.c) ---
    // On repart du chemin du bloc racine (raw_blocks_array[0].raw_block_directory_path)
    // et du nœud racine qu'on vient de créer : build_archive_subtree_from_raw_blocks
    // va peupler récursivement root_node->child_entries à partir de là.
    int build_result = build_archive_subtree_from_raw_blocks(
        raw_blocks_array, raw_block_count,
        raw_blocks_array[0].raw_block_directory_path, root_node);

    // Les RawHeaderBlock ne servent plus une fois la passe 2 terminée,
    // qu'elle ait réussi ou échoué : tout ce dont archive_parse() a
    // besoin a déjà été recopié dans l'arbre ArchiveNode (noms,
    // permissions, tailles — voir build_archive_subtree_from_raw_blocks).
    // On libère donc cette mémoire temporaire AVANT de vérifier le
    // résultat, pour ne jamais oublier de le faire sur l'un des deux
    // chemins (succès ou échec) qui suivent.
    free_raw_header_blocks(raw_blocks_array, raw_block_count);

    if (build_result < 0) {
        // L'arbre construit jusqu'ici (même partiel) est libéré par
        // archive_free, qui sait descendre récursivement quel que soit
        // le nombre d'enfants déjà attachés au moment de l'échec.
        archive_free(new_archive);
        return NULL;
    }

    return new_archive;
}


void archive_free(Archive *archive_to_free)
{
    // Permet d'appeler archive_free(NULL) sans risque — notamment utile
    // si archive_parse() avait déjà échoué avant même de créer
    // d'Archive, et que l'appelant fait quand même un archive_free par
    // précaution ou par symétrie de code.
    if (archive_to_free == NULL) {
        return;
    }
    // Délègue tout le travail récursif (enfants avant parents) à la
    // fonction dédiée de archive_tree_builder.c — archive_free() ne
    // fait que l'étape finale propre à Archive elle-même.
    free_archive_node_recursive(archive_to_free->root_entry);
    free(archive_to_free->source_file_path);
    free(archive_to_free);
}


#ifdef ARCHIVE_TEST_MAIN
// Harnais de test d'INTÉGRATION : vérifie que archive_parse/archive_free
// fonctionnent via la seule API publique. N'est jamais inclus dans la
// vraie compilation du projet via "make".
static void print_archive_tree_for_debugging(const ArchiveNode *node, int depth)
{
    // Indentation proportionnelle à la profondeur dans l'arbre, pour
    // visualiser la hiérarchie directement dans le texte affiché.
    for (int indent = 0; indent < depth; indent++) {
        printf("  ");
    }
    printf("%s%s  %s  %ld octets",
           node->entry_name,
           // Convention reprise du mode ls décrit dans le sujet : un "\"
           // après le nom signale un répertoire, rien après un fichier.
           node->entry_is_directory ? "\\" : "",
           node->entry_permissions,
           node->entry_size_in_bytes);
    if (!node->entry_is_directory && node->entry_size_in_bytes > 0) {
        // N'affiche la position dans le body QUE pour un fichier non
        // vide — un répertoire ou un fichier vide n'a jamais ces deux
        // champs renseignés (voir archive_tree_builder.c).
        printf("  (body: ligne %ld, %ld ligne(s))",
               node->file_content_start_line_in_body,
               node->file_content_line_count);
    }
    printf("\n");

    // Récursion : chaque enfant est affiché avec une profondeur +1,
    // donc une indentation supplémentaire. S'arrête naturellement sur
    // un fichier (child_entry_count == 0, la boucle ne s'exécute pas).
    for (size_t i = 0; i < node->child_entry_count; i++) {
        print_archive_tree_for_debugging(node->child_entries[i], depth + 1);
    }
}

int main(int argument_count, char *argument_values[])
{
    if (argument_count != 2) {
        fprintf(stderr, "Usage: %s <chemin_archive>\n", argument_values[0]);
        return 1;
    }

    // Seul point d'entrée utilisé ici : l'API PUBLIQUE, exactement
    // comme le ferait server.c plus tard — ce test ne triche pas en
    // appelant directement les fonctions internes des autres fichiers.
    Archive *archive = archive_parse(argument_values[1]);
    if (archive == NULL) {
        fprintf(stderr, "Erreur : impossible de parser %s\n", argument_values[1]);
        return 1;
    }

    printf("body_start_line_in_file = %ld\n\n", archive->body_start_line_in_file);
    print_archive_tree_for_debugging(archive->root_entry, 0);

    // Vérifie, de pair avec valgrind en externe, que toute la mémoire
    // allouée par archive_parse() est correctement libérée par
    // archive_free() — le message qui suit ne s'affiche que si
    // archive_free() ne plante pas en chemin.
    archive_free(archive);
    printf("\narchive_free() terminee sans crash.\n");
    return 0;
}
#endif