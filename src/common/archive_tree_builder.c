// ============================================================
//  PASSE 2 : transforme les blocs bruts de la passe 1
//  (archive_raw_parser.c) en un véritable arbre de ArchiveNode relié
//  par parent_directory / child_entries. Voir le diagramme "Détail de
//  la passe 2" pour le déroulement récursif complet.
// ============================================================

#include "archive_internal.h"
#include <stdlib.h>
#include <string.h>
// Note : plus besoin de <stdio.h> ici — build_child_directory_path()
// (et son usage de snprintf) a été déplacée dans archive_navigation.c,
// partagée via archive_internal.h.


static const RawHeaderBlock *find_raw_header_block_by_path(const RawHeaderBlock *raw_blocks_array,
                                                             size_t raw_blocks_array_count,
                                                             const char *target_directory_path)
{
    // Recherche LINÉAIRE (on parcourt le tableau du début à la fin),
    // volontairement simple plutôt qu'une structure plus rapide (table
    // de hachage, arbre trié...) : le nombre de répertoires dans une
    // archive de projet reste petit, et optimiser cette recherche
    // maintenant serait une optimisation prématurée pour un gain
    // négligeable en pratique.
    for (size_t block_index = 0; block_index < raw_blocks_array_count; block_index++) {
        // strcmp retourne 0 quand les deux chaînes sont IDENTIQUES
        // caractère par caractère — c'est une égalité stricte sur le
        // chemin complet, pas une comparaison de préfixe ou de nom
        // seul : c'est précisément ce qui permet de distinguer deux
        // répertoires portant le même nom simple à des endroits
        // différents de l'arbre (deux "data" dans deux branches
        // distinctes, par exemple).
        if (strcmp(raw_blocks_array[block_index].raw_block_directory_path, target_directory_path) == 0) {
            return &raw_blocks_array[block_index];
        }
    }
    // Aucun bloc ne porte ce chemin : soit l'appelant s'est trompé de
    // chemin, soit l'archive est corrompue (un sous-répertoire annoncé
    // sans bloc "directory" correspondant ailleurs dans le header).
    return NULL;
}


int build_archive_subtree_from_raw_blocks(const RawHeaderBlock *raw_blocks_array,
                                           size_t raw_blocks_array_count,
                                           const char *current_directory_path,
                                           ArchiveNode *current_directory_node)
{
    // Première étape : retrouver, parmi TOUS les blocs du header (sans
    // présumer de leur ordre dans le fichier), celui qui correspond au
    // répertoire qu'on est en train de peupler.
    const RawHeaderBlock *matching_block =
        find_raw_header_block_by_path(raw_blocks_array, raw_blocks_array_count, current_directory_path);

    if (matching_block == NULL) {
        // Un sous-répertoire a été ANNONCÉ par une entrée d'un bloc
        // parent (quelque part plus haut dans la récursion), mais aucun
        // bloc "directory" portant exactement ce chemin n'existe
        // ailleurs dans le header : l'archive est corrompue ou mal
        // formée, impossible de continuer à construire l'arbre ici.
        return -1;
    }

    // On parcourt chaque entrée du bloc retrouvé : chacune devient un
    // nouvel ArchiveNode, enfant direct de current_directory_node.
    for (size_t entry_index = 0; entry_index < matching_block->raw_block_entry_count; entry_index++) {
        const RawHeaderEntry *raw_entry = &matching_block->raw_block_entries[entry_index];

        // malloc(sizeof(ArchiveNode)) : on alloue UNE structure complète
        // sur le tas — sa durée de vie n'est pas limitée à cette
        // fonction, elle doit survivre tant que l'arbre existe, donc
        // pas question de la créer comme variable locale (qui serait
        // détruite à la sortie de cette fonction).
        ArchiveNode *new_child_node = malloc(sizeof(ArchiveNode));
        if (new_child_node == NULL) {
            return -1;
        }

        // Copie du nom avec sa propre allocation (strdup), indépendante
        // de raw_entry->raw_entry_name : raw_entry appartient aux blocs
        // bruts de la passe 1, qui seront libérés par archive_parse()
        // juste après cette construction — l'ArchiveNode a besoin de SA
        // PROPRE copie du nom pour rester valide après cette libération.
        new_child_node->entry_name = strdup(raw_entry->raw_entry_name);
        if (new_child_node->entry_name == NULL) {
            // Le malloc du nœud lui-même a réussi juste au-dessus, mais
            // pas celui de son nom : il faut libérer ce qui a déjà été
            // alloué avant de partir en erreur, sinon new_child_node
            // (désormais inaccessible, puisqu'on va retourner sans
            // jamais le rattacher à l'arbre) fuit.
            free(new_child_node);
            return -1;
        }

        // Le type (fichier/répertoire) se lit sur le premier caractère
        // des droits d'accès ('d' pour un répertoire, '-' pour un
        // fichier) — convention Unix déjà présente telle quelle dans
        // l'archive (voir le format du sujet), pas besoin d'un champ
        // séparé pour redire la même information autrement.
        new_child_node->entry_is_directory = (raw_entry->raw_entry_permissions[0] == 'd') ? 1 : 0;

        // entry_permissions est un tableau de taille FIXE (pas un
        // pointeur) dans ArchiveNode : une simple copie suffit, pas
        // besoin d'allocation séparée — contrairement à entry_name, dont
        // la longueur varie d'un fichier à l'autre.
        strncpy(new_child_node->entry_permissions, raw_entry->raw_entry_permissions, 10);
        new_child_node->entry_permissions[10] = '\0';

        new_child_node->entry_size_in_bytes = raw_entry->raw_entry_size_in_bytes;
        new_child_node->file_content_start_line_in_body = raw_entry->raw_entry_file_content_start_line_in_body;
        new_child_node->file_content_line_count = raw_entry->raw_entry_file_content_line_count;

        // Un répertoire tout juste créé n'a encore AUCUN enfant connu :
        // ils seront ajoutés un par un, plus bas dans cette même
        // fonction (appel récursif), au fur et à mesure qu'on découvre
        // son propre bloc "directory".
        new_child_node->child_entries = NULL;
        new_child_node->child_entry_count = 0;

        // C'est cette ligne, précisément, qui construit le lien vers le
        // PARENT — essentiel pour "cd .." et "pwd" plus tard, qui
        // remonteront l'arbre en suivant cette chaîne de pointeurs
        // plutôt que de recalculer un chemin stocké quelque part.
        new_child_node->parent_directory = current_directory_node;

        // On rattache ce nouveau nœud au tableau d'enfants DU PARENT
        // COURANT (current_directory_node, le vrai ArchiveNode qu'on
        // est en train de peupler — pas le bloc brut, qui n'a aucune
        // notion de parenté). realloc() agrandit ce tableau de
        // pointeurs d'un élément à chaque entrée traitée.
        ArchiveNode **grown_children_array = realloc(
            current_directory_node->child_entries,
            (current_directory_node->child_entry_count + 1) * sizeof(ArchiveNode *));
        if (grown_children_array == NULL) {
            // Même logique que plus haut : le nœud et son nom viennent
            // d'être alloués, mais ne sont encore rattachés à AUCUN
            // arbre — il faut les libérer explicitement ici avant de
            // partir en erreur, sinon ils fuient silencieusement.
            free(new_child_node->entry_name);
            free(new_child_node);
            return -1;
        }
        current_directory_node->child_entries = grown_children_array;
        current_directory_node->child_entries[current_directory_node->child_entry_count] = new_child_node;
        current_directory_node->child_entry_count++;

        if (new_child_node->entry_is_directory) {
            // C'est un sous-répertoire : il a lui-même un bloc "directory"
            // quelque part dans le header, qu'il faut retrouver par son
            // chemin COMPLET (pas juste son nom) — d'où la construction
            // de ce chemin avant de redescendre récursivement.
            char *child_directory_path =
                build_child_directory_path(current_directory_path, new_child_node->entry_name);
            if (child_directory_path == NULL) {
                return -1;
            }

            // Appel RÉCURSIF : on traite le sous-répertoire exactement
            // de la même façon que le répertoire courant, juste avec un
            // chemin plus profond et un nœud différent comme "nœud
            // courant". La récursion s'arrête naturellement quand un
            // répertoire n'a plus aucune entrée de type répertoire dans
            // son propre bloc (la boucle for ci-dessus se termine sans
            // déclencher de nouvel appel) — aucune condition d'arrêt
            // explicite à écrire séparément.
            int recursive_build_result = build_archive_subtree_from_raw_blocks(
                raw_blocks_array, raw_blocks_array_count,
                child_directory_path, new_child_node);

            // child_directory_path n'est qu'un chemin de travail
            // TEMPORAIRE, utile seulement le temps de cet appel récursif
            // précis — il n'est stocké nulle part dans l'ArchiveNode
            // (voir plus haut : le chemin complet se reconstruit via
            // parent_directory quand on en a besoin, jamais conservé
            // directement). On le libère donc dès que l'appel récursif
            // est terminé, qu'il ait réussi ou échoué.
            free(child_directory_path);

            if (recursive_build_result < 0) {
                // Propagation de l'échec : si un niveau PLUS PROFOND de
                // l'arbre a échoué (bloc introuvable, mémoire épuisée...),
                // ce niveau-ci ne peut pas prétendre avoir réussi non
                // plus — on relaie l'échec vers l'appelant, qui fera de
                // même, jusqu'à remonter à archive_parse().
                return -1;
            }
        }
        // Si ce n'est PAS un répertoire (entry_is_directory == 0), rien
        // de plus à faire pour cette entrée : pas de récursion, on
        // continue directement au tour de boucle suivant pour traiter
        // l'entrée suivante du même bloc.
    }
    return 0;
}


void free_archive_node_recursive(ArchiveNode *node_to_free)
{
    // Garde-fou : permet d'appeler cette fonction sur un pointeur NULL
    // sans risque (utile notamment si archive_free() est appelée sur une
    // Archive dont la construction a échoué en cours de route, où
    // root_entry pourrait être dans un état partiel).
    if (node_to_free == NULL) {
        return;
    }
    // Les enfants doivent être libérés AVANT le parent : une fois
    // node_to_free lui-même libéré par le free() plus bas, le tableau
    // node_to_free->child_entries (et donc l'accès à chaque enfant)
    // deviendrait un pointeur invalide — on ne pourrait alors plus du
    // tout les atteindre pour les libérer à leur tour. L'ordre de cette
    // fonction (enfants d'abord, structure du nœud ensuite) n'est donc
    // pas arbitraire.
    for (size_t child_index = 0; child_index < node_to_free->child_entry_count; child_index++) {
        // Récursion : chaque enfant peut lui-même avoir des enfants
        // (sous-répertoires), qui doivent être libérés avant lui pour
        // exactement la même raison. La récursion s'arrête naturellement
        // sur un fichier (child_entry_count vaut 0, la boucle ne
        // s'exécute pas) ou un répertoire vide.
        free_archive_node_recursive(node_to_free->child_entries[child_index]);
    }
    // Le tableau de POINTEURS vers les enfants (pas les enfants
    // eux-mêmes, déjà libérés individuellement ci-dessus).
    free(node_to_free->child_entries);
    // Le nom, alloué séparément par strdup() dans
    // build_archive_subtree_from_raw_blocks.
    free(node_to_free->entry_name);
    // Enfin la structure ArchiveNode elle-même.
    free(node_to_free);
}