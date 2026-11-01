// ============================================================
//  Fonctions qui MODIFIENT l'arbre ArchiveNode en mémoire ET le
//  fichier .arch sur le disque, pour les sous-commandes touch/mkdir/rm
//  du mode browse. Choix assumé : une modification en mode browse doit
//  affecter réellement l'archive persistante, pas seulement la session
//  de l'utilisateur connecté — sinon la commande n'aurait aucun effet
//  utile une fois la connexion fermée.
//
//  Chaque fonction ci-dessous modifie d'abord l'arbre EN MÉMOIRE, PUIS
//  appelle write_archive_to_file() (archive_persistence.c) avant de
//  retourner. Limite connue et assumée : si la modification en mémoire
//  réussit mais que la sauvegarde sur le disque échoue ensuite (disque
//  plein, droits insuffisants...), l'arbre en mémoire et le fichier sur
//  le disque divergent jusqu'à la prochaine sauvegarde réussie — aucun
//  mécanisme d'annulation automatique n'est implémenté ici.
// ============================================================

#include "archive.h"
#include "archive_internal.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_NEW_FILE_PERMISSIONS "-rw-r--r--"
#define DEFAULT_NEW_DIRECTORY_PERMISSIONS "drwxr-xr-x"
#define DEFAULT_NEW_DIRECTORY_SIZE_IN_BYTES 4096
// Valeurs conventionnelles assumées (comparables à un umask par défaut
// / une taille de bloc répertoire typique), jamais lues depuis
// l'archive elle-même.


int create_empty_file_in_directory(Archive *archive, ArchiveNode *parent_directory_node,
                                    const char *new_file_name)
{
    if (!parent_directory_node->entry_is_directory) {
        return -1;
    }

    // "touch toto1" sur un fichier déjà existant ne doit PAS échouer —
    // comme le vrai touch Unix. On sauvegarde quand même (par cohérence
    // d'API : l'appelant n'a jamais à se demander si une sauvegarde a eu
    // lieu ou non selon le cas), même si rien ne change concrètement
    // dans le fichier réécrit.
    for (size_t i = 0; i < parent_directory_node->child_entry_count; i++) {
        if (strcmp(parent_directory_node->child_entries[i]->entry_name, new_file_name) == 0) {
            return write_archive_to_file(archive);
        }
    }

    ArchiveNode *new_file_node = malloc(sizeof(ArchiveNode));
    if (new_file_node == NULL) {
        return -1;
    }

    new_file_node->entry_name = strdup(new_file_name);
    if (new_file_node->entry_name == NULL) {
        free(new_file_node);
        return -1;
    }

    new_file_node->entry_is_directory = 0;
    strcpy(new_file_node->entry_permissions, DEFAULT_NEW_FILE_PERMISSIONS);
    // Taille 0 et pas de position dans le body : exactement la même
    // convention qu'un fichier vide lu depuis une archive existante —
    // "un fichier vide a pour taille 0 et ne possède donc pas
    // d'informations complémentaires" (sujet §1.1).
    new_file_node->entry_size_in_bytes = 0;
    new_file_node->file_content_start_line_in_body = 0;
    new_file_node->file_content_line_count = 0;
    new_file_node->child_entries = NULL;
    new_file_node->child_entry_count = 0;
    new_file_node->parent_directory = parent_directory_node;

    // Même logique d'agrandissement du tableau d'enfants que dans
    // build_archive_subtree_from_raw_blocks : realloc() d'une unité,
    // le nouveau nœud prend la dernière case.
    ArchiveNode **grown_children_array = realloc(
        parent_directory_node->child_entries,
        (parent_directory_node->child_entry_count + 1) * sizeof(ArchiveNode *));
    if (grown_children_array == NULL) {
        free(new_file_node->entry_name);
        free(new_file_node);
        return -1;
    }
    parent_directory_node->child_entries = grown_children_array;
    parent_directory_node->child_entries[parent_directory_node->child_entry_count] = new_file_node;
    parent_directory_node->child_entry_count++;

    // La modification en mémoire est terminée et cohérente : on tente
    // maintenant de la rendre PERMANENTE. Si ça échoue, le nœud reste
    // dans l'arbre en mémoire (pas de retrait automatique — voir la
    // limite assumée en tête de fichier), mais l'appelant est informé
    // de l'échec via la valeur de retour.
    return write_archive_to_file(archive);
}


int create_subdirectory_in_directory(Archive *archive, ArchiveNode *parent_directory_node,
                                      const char *new_directory_name)
{
    if (!parent_directory_node->entry_is_directory) {
        return -1;
    }

    // Contrairement à touch, mkdir DOIT échouer si le nom existe déjà
    // (fichier ou répertoire) — comportement standard Unix. On retourne
    // -1 directement, SANS appeler write_archive_to_file : rien n'a
    // changé, une sauvegarde serait un travail inutile (et réécrirait le
    // fichier à l'identique pour rien).
    for (size_t i = 0; i < parent_directory_node->child_entry_count; i++) {
        if (strcmp(parent_directory_node->child_entries[i]->entry_name, new_directory_name) == 0) {
            return -1;
        }
    }

    ArchiveNode *new_directory_node = malloc(sizeof(ArchiveNode));
    if (new_directory_node == NULL) {
        return -1;
    }

    new_directory_node->entry_name = strdup(new_directory_name);
    if (new_directory_node->entry_name == NULL) {
        free(new_directory_node);
        return -1;
    }

    new_directory_node->entry_is_directory = 1;
    strcpy(new_directory_node->entry_permissions, DEFAULT_NEW_DIRECTORY_PERMISSIONS);
    new_directory_node->entry_size_in_bytes = DEFAULT_NEW_DIRECTORY_SIZE_IN_BYTES;
    new_directory_node->file_content_start_line_in_body = 0;
    new_directory_node->file_content_line_count = 0;
    // NULL + 0 : un répertoire tout juste créé n'a aucun enfant — c'est
    // une feuille de l'arbre jusqu'à ce qu'on y fasse touch/mkdir dedans.
    new_directory_node->child_entries = NULL;
    new_directory_node->child_entry_count = 0;
    new_directory_node->parent_directory = parent_directory_node;

    ArchiveNode **grown_children_array = realloc(
        parent_directory_node->child_entries,
        (parent_directory_node->child_entry_count + 1) * sizeof(ArchiveNode *));
    if (grown_children_array == NULL) {
        free(new_directory_node->entry_name);
        free(new_directory_node);
        return -1;
    }
    parent_directory_node->child_entries = grown_children_array;
    parent_directory_node->child_entries[parent_directory_node->child_entry_count] = new_directory_node;
    parent_directory_node->child_entry_count++;

    return write_archive_to_file(archive);
}


int remove_entry_from_tree(Archive *archive, ArchiveNode *entry_to_remove)
{
    // La racine n'a pas de parent (voir archive_parse) : il n'existe
    // aucun tableau "child_entries" dont elle pourrait être retirée, et
    // la supprimer rendrait Archive->root_entry invalide. On refuse ce
    // cas plutôt que de risquer de corrompre toute l'archive.
    if (entry_to_remove->parent_directory == NULL) {
        return -1;
    }

    ArchiveNode *parent = entry_to_remove->parent_directory;

    // On retrouve l'INDEX de entry_to_remove dans le tableau de son
    // parent, par comparaison de POINTEUR (pas de nom) : on a déjà le
    // nœud exact à supprimer (résolu au préalable par
    // find_node_by_path côté appelant), pas besoin de le rechercher par
    // son nom une seconde fois.
    int found_index = -1;
    for (size_t i = 0; i < parent->child_entry_count; i++) {
        if (parent->child_entries[i] == entry_to_remove) {
            found_index = (int)i;
            break;
        }
    }

    if (found_index == -1) {
        // Ne devrait normalement jamais arriver (ça signifierait que
        // entry_to_remove->parent_directory pointe vers un nœud qui ne
        // le liste pourtant pas parmi ses enfants — un arbre
        // incohérent) : on refuse par sécurité plutôt que de continuer
        // sur une structure suspecte.
        return -1;
    }

    // Décale tous les éléments APRÈS found_index d'une case vers la
    // gauche, pour combler le "trou" laissé par l'élément supprimé —
    // préserve l'ordre relatif des autres enfants, celui-là même qui
    // reproduit l'ordre d'apparition dans le fichier d'archive
    // d'origine (voir le format du header).
    for (size_t i = (size_t)found_index; i + 1 < parent->child_entry_count; i++) {
        parent->child_entries[i] = parent->child_entries[i + 1];
    }
    parent->child_entry_count--;

    if (parent->child_entry_count == 0) {
        // Dernier enfant retiré : le tableau redevient vide. On libère
        // explicitement plutôt que de garder un tableau de taille 0 —
        // cohérent avec l'état "NULL + 0" utilisé partout ailleurs pour
        // représenter un répertoire sans enfants.
        free(parent->child_entries);
        parent->child_entries = NULL;
    } else {
        // Réduction de la taille allouée pour refléter le nouveau
        // nombre d'enfants. Purement une question de propreté mémoire :
        // même si ce realloc échouait (cas quasiment impossible pour un
        // RÉTRÉCISSEMENT), l'ancien bloc — plus grand que nécessaire
        // mais toujours valide — resterait parfaitement utilisable, on
        // ignore donc un éventuel échec ici plutôt que de faire échouer
        // toute la suppression pour un détail purement cosmétique.
        ArchiveNode **shrunk_array = realloc(
            parent->child_entries, parent->child_entry_count * sizeof(ArchiveNode *));
        if (shrunk_array != NULL) {
            parent->child_entries = shrunk_array;
        }
    }

    // free_archive_node_recursive DOIT libérer le nœud AVANT que la
    // sauvegarde ne parcoure l'arbre : le nœud supprimé n'apparaît déjà
    // plus dans child_entries, donc serialize_directory_block ne le
    // verra jamais — l'ordre entre ce free et write_archive_to_file
    // n'a donc pas d'incidence fonctionnelle, seulement une question de
    // "nettoyer la mémoire dès qu'on le peut".
    free_archive_node_recursive(entry_to_remove);

    return write_archive_to_file(archive);
}