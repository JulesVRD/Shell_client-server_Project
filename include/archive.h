#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stddef.h>

// ============================================================
//  Représentation EN MÉMOIRE d'une archive une fois son header
//  (à plat dans le fichier) reconstruit en un véritable arbre.
//  Une instance d'ArchiveNode = un seul fichier OU un seul
//  répertoire. Cette structure reste vivante pendant toute la
//  durée de la session de browse, ce n'est pas une structure
//  jetable utilisée seulement pendant le parsing.
// ============================================================

typedef struct ArchiveNode {

    char *entry_name;
    // Nom SEUL de ce fichier ou répertoire (ex: "toto1"), jamais
    // le chemin complet. Pointeur car la longueur du nom est
    // inconnue à l'avance et variable d'une entrée à l'autre.

    int entry_is_directory;
    // 1 si cette entrée est un répertoire, 0 si c'est un fichier.
    // Déduit du premier caractère du champ "droits" lu dans
    // l'archive ('d' pour un répertoire, '-' pour un fichier).

    char entry_permissions[11];
    // Droits d'accès copiés tels quels depuis l'archive, ex:
    // "drwxr-x--x" ou "-rw-r--r--". 10 caractères + '\0' final.

    long entry_size_in_bytes;
    // Taille en octets telle que donnée dans l'archive.
    // Vaut 0 pour un répertoire ou pour un fichier vide.

    long file_content_start_line_in_body;
    // UNIQUEMENT SI entry_is_directory == 0 ET entry_size_in_bytes > 0.
    // Numéro de ligne où commence le contenu de ce fichier,
    // RELATIF au début du body (pas une ligne absolue du fichier
    // .arch entier).

    long file_content_line_count;
    // UNIQUEMENT SI entry_is_directory == 0 ET entry_size_in_bytes > 0.
    // Nombre de lignes que ce fichier occupe dans le body.

    struct ArchiveNode **child_entries;
    // UNIQUEMENT SI entry_is_directory == 1.
    // Tableau de POINTEURS vers les enfants directs (fichiers ou
    // sous-répertoires immédiats). Pointeur double car : (1) un
    // ArchiveNode ne peut pas contenir directement d'autres
    // ArchiveNode par valeur (taille infinie à calculer), et (2)
    // le nombre d'enfants est variable, donc ce tableau est alloué
    // dynamiquement (malloc/realloc) et peut grandir avec mkdir/touch.

    size_t child_entry_count;
    // Nombre d'éléments actuellement dans child_entries.
    // UNIQUEMENT SI entry_is_directory == 1 (vaut 0 sinon).

    struct ArchiveNode *parent_directory;
    // Pointeur vers le répertoire parent direct. NULL uniquement
    // pour la racine de l'archive. Permet de remonter l'arbre pour
    // implémenter "cd .." et reconstruire le chemin pour "pwd"
    // sans avoir à stocker le chemin complet dans chaque nœud.

} ArchiveNode;


// ============================================================
//  Une archive complète = un fichier .arch chargé en mémoire.
//  Une instance d'Archive par session de browse. Deux clients
//  qui parcourent la même archive en même temps obtiennent CHACUN
//  leur propre Archive, totalement indépendante de l'autre.
// ============================================================

typedef struct Archive {

    ArchiveNode *root_entry;
    // Pointeur vers le nœud racine de l'arbre, correspondant au
    // tout premier bloc "directory" du header de l'archive.

    long body_start_line_in_file;
    // Numéro de ligne ABSOLU (dans le fichier .arch entier) où
    // commence le body. Lu directement sur la ligne 1 de
    // l'archive (le "25" dans l'exemple "3:25" du sujet). Mis à
    // jour après chaque sauvegarde réussie (voir
    // archive_persistence.c), pour refléter le fichier tel qu'il
    // existe RÉELLEMENT sur le disque à tout moment.

    char *source_file_path;
    // Chemin du fichier .arch d'origine, conservé pour pouvoir le
    // RELIRE plus tard (lire le contenu d'un fichier a besoin de
    // retourner au fichier, voir archive_body.c) et le RÉÉCRIRE
    // après une modification (voir archive_persistence.c). Sans ce
    // champ, Archive ne saurait plus du tout d'où elle vient une
    // fois archive_parse() terminée.

} Archive;


// ============================================================
//  API principale : charger une archive depuis le disque, la
//  libérer de la mémoire. Voir archive.c, archive_raw_parser.c
//  (passe 1) et archive_tree_builder.c (passe 2) pour le détail.
// ============================================================

Archive *archive_parse(const char *archive_file_path);
// Parse le fichier à archive_file_path et construit l'arbre en
// mémoire. Retourne un pointeur alloué (à libérer avec
// archive_free()), ou NULL si le fichier est introuvable ou mal
// formé.

void archive_free(Archive *archive_to_free);
// Libère récursivement tout l'arbre, puis la structure Archive
// elle-même.


// ============================================================
//  Navigation en LECTURE SEULE dans l'arbre : pwd, ls, et la
//  résolution de chemin utilisée par cd. Voir archive_navigation.c.
// ============================================================

ArchiveNode *find_node_by_path(ArchiveNode *current_directory_node,
                                ArchiveNode *archive_root_node,
                                const char *path_to_resolve);
// Résout path_to_resolve en partant de archive_root_node si le
// chemin est ABSOLU (commence par '\'), ou de
// current_directory_node s'il est RELATIF. Gère ".." (remonter
// d'un niveau, sans erreur si déjà à la racine) et "." (rester sur
// place). Retourne le ArchiveNode trouvé (fichier OU répertoire),
// ou NULL si un composant du chemin est introuvable, ou si le
// chemin tente de "traverser" un fichier comme s'il s'agissait
// d'un répertoire.

char *build_absolute_path_string(const ArchiveNode *node);
// Construit le chemin absolu complet de node, depuis la racine —
// toujours représentée par "\", jamais par le chemin réel d'origine
// de la machine qui a fait -create. Retourne une chaîne allouée, à
// libérer par l'appelant avec free(). Utilisée pour implémenter pwd.

int entry_name_starts_with_dot(const ArchiveNode *entry);
// 1 si le nom de l'entrée commence par '.' (convention Unix de
// fichier "caché", pour filtrer ls selon l'option -a), 0 sinon.

int entry_has_any_execute_permission(const ArchiveNode *entry);
// 1 si au moins un des 3 bits d'exécution (propriétaire/groupe/
// autres) est actif — détermine le suffixe "*" du ls sans option.

void format_ls_short_name(const ArchiveNode *entry, char *output_buffer, size_t output_buffer_capacity);
// Formate le nom d'une entrée façon "ls" sans option : "\" pour un
// répertoire, "*" pour un fichier exécutable, rien sinon.

void format_ls_long_line(const ArchiveNode *entry, char *output_buffer, size_t output_buffer_capacity);
// Formate une entrée façon "ls -l" : droits, puis taille, puis nom
// (ordre d'AFFICHAGE, différent de l'ordre de STOCKAGE dans le
// fichier .arch lui-même — voir archive_navigation.c).


// ============================================================
//  Lecture du CONTENU d'un fichier (pour cat). Voir archive_body.c.
// ============================================================

char *read_file_content_from_archive(const Archive *archive, const ArchiveNode *file_node);
// Relit, dans archive->source_file_path, le contenu réel du
// fichier représenté par file_node (en utilisant sa position dans
// le body). Retourne une chaîne allouée (à libérer par
// l'appelant), une chaîne vide "" pour un fichier de taille 0, ou
// NULL si file_node est un répertoire ou si la lecture échoue
// (fichier introuvable, archive tronquée par rapport à ce que le
// header annonçait).


// ============================================================
//  Sauvegarde de l'arbre courant vers le disque. Voir
//  archive_persistence.c.
// ============================================================

int write_archive_to_file(Archive *archive);
// Régénère ENTIÈREMENT le fichier archive->source_file_path à
// partir de l'arbre ArchiveNode actuel (header ET body recalculés
// de zéro — nécessaire car ajouter/retirer un fichier décale la
// position de tous les fichiers qui le suivent dans le body).
// Écrit d'abord dans un fichier temporaire puis le renomme à la
// place de l'original : en cas d'échec en cours d'écriture, le
// fichier d'origine reste intact. Met à jour
// archive->body_start_line_in_file et la position de chaque
// fichier dans l'arbre pour refléter le fichier nouvellement
// écrit. Retourne 0 si succès, -1 sinon (le fichier d'origine
// n'est alors pas modifié).


// ============================================================
//  Mutation de l'arbre : touch, mkdir, rm. Voir archive_mutation.c.
//  Chacune de ces 3 fonctions modifie l'arbre EN MÉMOIRE puis
//  appelle write_archive_to_file() avant de retourner : une
//  modification en mode browse affecte donc réellement le fichier
//  .arch sur le disque, pas seulement la session en cours (choix
//  assumé : sans ça, une commande n'aurait plus aucun effet une
//  fois la connexion fermée).
// ============================================================

int create_empty_file_in_directory(Archive *archive, ArchiveNode *parent_directory_node,
                                    const char *new_file_name);
// Crée un fichier vide nommé new_file_name dans
// parent_directory_node, puis sauvegarde l'archive. Si un fichier
// ou répertoire de ce nom existe déjà, ne modifie rien mais
// sauvegarde quand même et retourne 0 (comme le vrai touch Unix).
// Retourne -1 si parent_directory_node n'est pas un répertoire, en
// cas d'échec d'allocation, ou si la sauvegarde échoue.

int create_subdirectory_in_directory(Archive *archive, ArchiveNode *parent_directory_node,
                                      const char *new_directory_name);
// Crée un répertoire vide nommé new_directory_name dans
// parent_directory_node, puis sauvegarde l'archive. Retourne -1
// (et ne modifie rien) si un fichier ou répertoire de ce nom
// existe déjà, si parent_directory_node n'est pas un répertoire,
// en cas d'échec d'allocation, ou si la sauvegarde échoue.
// Retourne 0 si succès.

int remove_entry_from_tree(Archive *archive, ArchiveNode *entry_to_remove);
// Retire entry_to_remove de son parent et libère tout son
// sous-arbre, puis sauvegarde l'archive. Retourne -1 si
// entry_to_remove est la racine de l'archive (ne peut pas être
// supprimée), ou si la sauvegarde échoue. Retourne 0 si succès.

#endif