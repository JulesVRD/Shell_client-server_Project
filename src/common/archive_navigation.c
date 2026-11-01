// ============================================================
//  Fonctions d'exploitation de l'arbre ArchiveNode une fois construit,
//  pour les sous-commandes du mode browse : pwd, ls, et la résolution
//  de chemin utilisée par cd. Contrairement à archive_raw_parser.c et
//  archive_tree_builder.c, ces fonctions sont déclarées dans archive.h
//  (PAS archive_internal.h) : server.c en a directement besoin.
// ============================================================

#include "archive.h"
#include "archive_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *build_child_directory_path(const char *parent_directory_path,
                                  const char *child_directory_name)
{
    // Déplacée depuis archive_tree_builder.c : la passe 2 l'utilisait
    // déjà pour DESCENDRE vers les feuilles (chemin du parent + nom de
    // l'enfant = chemin de l'enfant). build_absolute_path_string,
    // plus bas dans ce fichier, a besoin de la MÊME opération, utilisée
    // cette fois pour REMONTER vers la racine (pwd) — la jonction
    // "chemin + nom = chemin plus profond" est rigoureusement identique
    // dans les deux sens, donc partagée ici plutôt que dupliquée dans
    // les deux fichiers.
    size_t parent_path_length = strlen(parent_directory_path);

    // Cas particulier à détecter AVANT de construire la chaîne : le
    // chemin de la RACINE ("Exemple\Test\" dans l'exemple du sujet) se
    // termine déjà par un backslash, alors que les niveaux plus
    // profonds ("Exemple\Test\A") n'en ont pas. Sans cette vérification,
    // on obtiendrait soit un double backslash soit un chemin collé sans
    // séparateur selon le niveau — les deux cassent toute comparaison
    // stricte de chemin faite ailleurs dans le programme.
    int parent_path_already_ends_with_separator =
        (parent_path_length > 0 && parent_directory_path[parent_path_length - 1] == '\\');

    size_t needed_buffer_size = parent_path_length + strlen(child_directory_name)
                               + (parent_path_already_ends_with_separator ? 0 : 1) + 1;

    char *child_directory_path = malloc(needed_buffer_size);
    if (child_directory_path == NULL) {
        return NULL;
    }

    if (parent_path_already_ends_with_separator) {
        snprintf(child_directory_path, needed_buffer_size, "%s%s",
                 parent_directory_path, child_directory_name);
    } else {
        snprintf(child_directory_path, needed_buffer_size, "%s\\%s",
                 parent_directory_path, child_directory_name);
    }
    return child_directory_path;
}


char *build_absolute_path_string(const ArchiveNode *node)
{
    if (node->parent_directory == NULL) {
        // Nœud racine : son propre nom (le chemin réel "Exemple\Test\"
        // d'origine, voir ArchiveNode->entry_name pour ce nœud précis)
        // n'est PAS ce qu'il faut afficher à l'utilisateur de vsh. Le
        // sujet est explicite : la racine du shell est toujours "\",
        // quel que soit le chemin d'origine sur la machine qui a fait
        // -create. On ignore donc délibérément entry_name ici.
        return strdup("\\");
    }

    // On construit D'ABORD le chemin du PARENT en remontant récursivement
    // jusqu'à la racine, puis on colle le nom du nœud courant dessus en
    // dernier. L'appel récursif redescend la pile d'appels jusqu'au cas
    // de base ci-dessus (la racine), puis chaque niveau rajoute son
    // propre nom en remontant — résultat : le chemin final est bien
    // construit de la racine vers la feuille, dans le bon ordre, même si
    // la récursion elle-même part de la feuille vers la racine.
    char *parent_path = build_absolute_path_string(node->parent_directory);
    if (parent_path == NULL) {
        // Échec d'allocation quelque part plus haut dans la récursion :
        // on relaie l'échec sans tenter de construire quoi que ce soit
        // de partiel.
        return NULL;
    }

    char *full_path = build_child_directory_path(parent_path, node->entry_name);

    // parent_path n'était qu'un résultat INTERMÉDIAIRE, utile seulement
    // pour construire full_path : il ne doit pas fuir une fois cette
    // jonction faite. Même logique que child_directory_path dans
    // build_archive_subtree_from_raw_blocks, qui ne survit jamais plus
    // longtemps que nécessaire.
    free(parent_path);

    return full_path;
    // Si build_child_directory_path a échoué (NULL), on retourne NULL
    // tel quel — pas besoin de vérification supplémentaire ici, le
    // free(parent_path) juste au-dessus reste correct dans tous les cas
    // (parent_path n'est jamais NULL à ce stade, seul full_path peut
    // l'être).
}


ArchiveNode *find_node_by_path(ArchiveNode *current_directory_node,
                                ArchiveNode *archive_root_node,
                                const char *path_to_resolve)
{
    // strtok() MODIFIE la chaîne qu'on lui passe (elle y insère des
    // '\0' à la place de chaque séparateur trouvé) — on ne peut donc
    // jamais l'utiliser directement sur path_to_resolve, qui est
    // 'const' précisément parce que l'appelant ne s'attend pas à ce que
    // sa propre chaîne soit modifiée derrière son dos. On travaille sur
    // une COPIE, jetable à la fin de cette fonction.
    char *path_copy = strdup(path_to_resolve);
    if (path_copy == NULL) {
        return NULL;
    }

    // Chemin ABSOLU (commence par "\") → on repart de la racine de
    // l'archive, peu importe où on se trouvait avant cet appel. Chemin
    // RELATIF (tout le reste) → on repart du répertoire courant de la
    // session de browse en cours.
    ArchiveNode *current_node =
        (path_to_resolve[0] == '\\') ? archive_root_node : current_directory_node;

    // strtok avec "\\" comme délimiteur saute automatiquement les
    // séparateurs en début de chaîne ET les séparateurs consécutifs :
    // un chemin absolu "\A\B" donne directement les tokens "A" puis "B"
    // (le premier "\" est traité comme un simple séparateur de tête,
    // pas comme un token vide). C'est aussi ce qui fait qu'un chemin
    // "\" tout seul, ou une chaîne vide "", ne produit AUCUN token : la
    // boucle ci-dessous ne s'exécute alors pas une seule fois, et
    // current_node (déjà fixé juste au-dessus) est retourné tel
    // quel — exactement le comportement voulu pour "aller à la racine"
    // ou "rester sur place", sans avoir à écrire ces deux cas à part.
    char *path_component = strtok(path_copy, "\\");

    while (path_component != NULL) {

        if (strcmp(path_component, "..") == 0) {
            // Remonter d'un niveau. Si on est déjà à la racine
            // (parent_directory == NULL), on reste sur place plutôt que
            // d'échouer — comportement calqué sur un shell Unix
            // classique, où "cd .." à la racine ne produit pas d'erreur,
            // cohérent avec la demande du sujet de reproduire "le même
            // comportement que celles du shell classique".
            if (current_node->parent_directory != NULL) {
                current_node = current_node->parent_directory;
            }

        } else if (strcmp(path_component, ".") == 0) {
            // "Rester sur place" : absent de l'énoncé, mais cohérent
            // avec un shell classique et ne coûte rien à gérer — évite
            // une erreur surprenante si "cd ." est tapé par réflexe.
            // (current_node ne change pas, rien à faire ici)

        } else {
            // Recherche de path_component PARMI LES ENFANTS DIRECTS du
            // nœud courant uniquement — jamais plus loin d'un coup, la
            // boucle while se charge de descendre niveau par niveau.
            //
            // Point important : si current_node est un FICHIER (pas un
            // répertoire), child_entry_count vaut 0 (voir
            // build_archive_subtree_from_raw_blocks, qui ne remplit ce
            // champ que pour les répertoires). Cette boucle ne trouve
            // alors jamais rien, et la fonction échoue naturellement au
            // bloc "if (matching_child == NULL)" plus bas — sans qu'on
            // ait eu besoin d'écrire une vérification séparée du genre
            // "le composant précédent est-il bien un répertoire ?". Le
            // cas "cd toto doit échouer si toto est un fichier" (exigé
            // par le sujet) est donc couvert par construction, pas par
            // un test explicite.
            ArchiveNode *matching_child = NULL;
            for (size_t i = 0; i < current_node->child_entry_count; i++) {
                if (strcmp(current_node->child_entries[i]->entry_name, path_component) == 0) {
                    matching_child = current_node->child_entries[i];
                    break;
                }
            }

            if (matching_child == NULL) {
                // Composant introuvable : le chemin est invalide dans
                // son ensemble, peu importe que les composants
                // précédents aient été valides.
                free(path_copy);
                return NULL;
            }
            current_node = matching_child;
        }

        // Passer NULL (plutôt que path_copy) signale à strtok qu'on
        // continue de découper la MÊME chaîne que le premier appel :
        // strtok garde en mémoire, en interne, la position où il s'est
        // arrêté la dernière fois.
        path_component = strtok(NULL, "\\");
    }

    free(path_copy);
    return current_node;
}


int entry_name_starts_with_dot(const ArchiveNode *entry)
{
    // Convention identique à celle des systèmes Unix : un nom qui
    // commence par '.' est considéré "caché", et n'apparaît dans ls que
    // si l'option -a est demandée. Rien dans le format d'archive lui-même
    // ne distingue un fichier "caché" d'un autre — cette convention
    // s'applique simplement au nom tel qu'il a été stocké, exactement
    // comme le ferait un vrai shell Unix sur un système de fichiers réel.
    return entry->entry_name[0] == '.';
}


int entry_has_any_execute_permission(const ArchiveNode *entry)
{
    // Dans une chaîne de droits à 10 caractères ("-rwxr-xr-x"), les
    // positions 3, 6 et 9 correspondent respectivement aux droits
    // d'exécution du propriétaire, du groupe, et des autres —
    // convention Unix standard (index 0 = type, 1-3 = propriétaire,
    // 4-6 = groupe, 7-9 = autres). "ls -F" (dont le sujet reprend le
    // comportement) marque d'un "*" un fichier dès que N'IMPORTE LEQUEL
    // des trois est actif, pas seulement celui du propriétaire.
    return entry->entry_permissions[3] == 'x'
        || entry->entry_permissions[6] == 'x'
        || entry->entry_permissions[9] == 'x';
}


void format_ls_short_name(const ArchiveNode *entry, char *output_buffer, size_t output_buffer_capacity)
{
    // Reproduit exactement le format de "ls" SANS option montré dans le
    // sujet (§1.4) : un répertoire est suffixé par "\", un fichier
    // exécutable par "*", un fichier normal n'a aucun suffixe.
    const char *suffix = "";
    if (entry->entry_is_directory) {
        suffix = "\\";
    } else if (entry_has_any_execute_permission(entry)) {
        suffix = "*";
    }
    // snprintf borne l'écriture à output_buffer_capacity quoi qu'il
    // arrive, même si le nom de l'entrée était anormalement long — la
    // responsabilité de fournir un buffer de taille raisonnable
    // (MAX_PROTOCOL_LINE_LENGTH, défini dans protocol.h, convient
    // largement) revient à l'appelant.
    snprintf(output_buffer, output_buffer_capacity, "%s%s", entry->entry_name, suffix);
}


void format_ls_long_line(const ArchiveNode *entry, char *output_buffer, size_t output_buffer_capacity)
{
    // Reproduit l'ordre des colonnes montré par "ls -l" dans le sujet :
    // droits, puis taille, puis nom — À NE PAS CONFONDRE avec l'ordre
    // nom/droits/taille utilisé DANS LE FICHIER D'ARCHIVE lui-même (voir
    // parse_raw_header_entry_line dans archive_raw_parser.c). Ce sont
    // deux formats différents avec deux objectifs différents : l'un est
    // un format de STOCKAGE sur disque, l'autre un format d'AFFICHAGE à
    // l'écran — les confondre serait une source de bug facile si on
    // réutilisait la même fonction pour les deux.
    snprintf(output_buffer, output_buffer_capacity, "%s  %ld  %s",
             entry->entry_permissions,
             entry->entry_size_in_bytes,
             entry->entry_name);
}


#ifdef ARCHIVE_NAVIGATION_TEST_MAIN
// Harnais de test d'intégration : vérifie pwd/ls/cd directement contre
// les exemples donnés dans le sujet (§1.4), chiffres et chemins inclus.
// N'est jamais inclus dans la vraie compilation du projet via "make".
int main(int argument_count, char *argument_values[])
{
    if (argument_count != 2) {
        fprintf(stderr, "Usage: %s <chemin_archive>\n", argument_values[0]);
        return 1;
    }

    Archive *archive = archive_parse(argument_values[1]);
    if (archive == NULL) {
        fprintf(stderr, "Erreur : impossible de parser %s\n", argument_values[1]);
        return 1;
    }

    // La "session de browse" est simulée ici par une simple variable
    // locale : dans le vrai programme, ce sera un champ d'état propre à
    // chaque CLIENT connecté côté serveur (voir server.c, à venir).
    ArchiveNode *current_directory = archive->root_entry;

    // --- pwd à la racine ---
    char *path = build_absolute_path_string(current_directory);
    printf("pwd (racine)       : %s   (attendu: \\)\n", path);
    free(path);

    // --- ls à la racine (attendu du sujet : "A\ B\ toto1* toto2") ---
    printf("ls (racine)        : ");
    for (size_t i = 0; i < current_directory->child_entry_count; i++) {
        char entry_buffer[128];
        format_ls_short_name(current_directory->child_entries[i], entry_buffer, sizeof(entry_buffer));
        printf("%s ", entry_buffer);
    }
    printf("  (attendu: A\\ B\\ toto1* toto2 )\n");

    // --- cd A puis pwd (attendu: \A) ---
    ArchiveNode *node_A = find_node_by_path(current_directory, archive->root_entry, "A");
    if (node_A == NULL) {
        fprintf(stderr, "ECHEC : cd A n'a pas trouve le repertoire\n");
        archive_free(archive);
        return 1;
    }
    current_directory = node_A;
    path = build_absolute_path_string(current_directory);
    printf("cd A puis pwd      : %s   (attendu: \\A)\n", path);
    free(path);

    // --- ls -l dans A (attendu du sujet : droits/taille/nom, un par ligne) ---
    printf("ls -l (dans A)     :\n");
    for (size_t i = 0; i < current_directory->child_entry_count; i++) {
        char entry_buffer[128];
        format_ls_long_line(current_directory->child_entries[i], entry_buffer, sizeof(entry_buffer));
        printf("  %s\n", entry_buffer);
    }

    // --- cd A1 (relatif, depuis A) puis pwd (attendu: \A\A1) ---
    ArchiveNode *node_A1 = find_node_by_path(current_directory, archive->root_entry, "A1");
    if (node_A1 == NULL) {
        fprintf(stderr, "ECHEC : cd A1 n'a pas trouve le repertoire\n");
        archive_free(archive);
        return 1;
    }
    current_directory = node_A1;
    path = build_absolute_path_string(current_directory);
    printf("cd A1 puis pwd     : %s   (attendu: \\A\\A1)\n", path);
    free(path);

    // --- cd ..\.. (remonter deux niveaux d'un coup) puis pwd (attendu: \) ---
    ArchiveNode *node_back_to_root = find_node_by_path(current_directory, archive->root_entry, "..\\..");
    current_directory = node_back_to_root;
    path = build_absolute_path_string(current_directory);
    printf("cd ..\\.. puis pwd  : %s   (attendu: \\)\n", path);
    free(path);

    // --- cd A\A1 en un seul chemin absolu, directement depuis la racine ---
    ArchiveNode *node_direct = find_node_by_path(current_directory, archive->root_entry, "\\A\\A1");
    path = build_absolute_path_string(node_direct);
    printf("cd \\A\\A1 (absolu)  : %s   (attendu: \\A\\A1)\n", path);
    free(path);

    // --- cas d'erreur : répertoire inexistant ---
    ArchiveNode *node_missing = find_node_by_path(archive->root_entry, archive->root_entry, "Z");
    printf("cd Z (inexistant)  : %s   (attendu: echec/NULL)\n",
           node_missing == NULL ? "NULL (echec correct)" : "TROUVE (BUG)");

    // --- cas d'erreur : essayer de descendre DANS un fichier ---
    ArchiveNode *node_through_file =
        find_node_by_path(archive->root_entry, archive->root_entry, "toto1\\quelquechose");
    printf("cd toto1\\x (fichier): %s   (attendu: echec/NULL)\n",
           node_through_file == NULL ? "NULL (echec correct)" : "TROUVE (BUG)");

    archive_free(archive);
    return 0;
}
#endif