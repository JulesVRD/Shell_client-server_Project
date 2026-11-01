// ============================================================
//  Lecture du CONTENU réel d'un fichier, à partir de sa position dans
//  le body de l'archive (file_content_start_line_in_body /
//  file_content_line_count, stockés dans ArchiveNode). Nécessaire pour
//  "cat", et pour archive_persistence.c qui doit relire le contenu de
//  chaque fichier non modifié avant de réécrire toute l'archive.
// ============================================================

#include "archive.h"
#include "archive_internal.h"
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_BODY_LINE_MAX_LENGTH 1024
// Taille maximale d'une ligne de CONTENU dans le body. Constante
// séparée de ARCHIVE_HEADER_LINE_MAX_LENGTH (dans archive_raw_parser.c)
// bien qu'elles vaillent la même chose actuellement : l'une borne des
// lignes de métadonnées (noms, droits...), l'autre des lignes de texte
// quelconque à l'intérieur des fichiers — deux choses conceptuellement
// différentes, qui pourraient avoir des limites différentes un jour.


char *read_file_content_from_archive(const Archive *archive, const ArchiveNode *file_node)
{
    if (file_node->entry_is_directory) {
        // Un répertoire n'a pas de "contenu" au sens où un fichier en a
        // un — rien à lire dans le body pour lui.
        return NULL;
    }

    if (file_node->entry_size_in_bytes == 0) {
        // Fichier vide : pas de position dans le body à chercher (voir
        // archive_raw_parser.c — ces champs valent 0 et ne SIGNIFIENT
        // rien pour un fichier vide, ce n'est pas "une position 0").
        // strdup("") alloue quand même une vraie chaîne (1 octet, le
        // '\0'), pour que l'appelant puisse la libérer exactement comme
        // n'importe quel autre résultat de cette fonction, sans avoir à
        // distinguer "chaîne vide allouée" de "pas de résultat du tout".
        return strdup("");
    }

    FILE *archive_file = fopen(archive->source_file_path, "r");
    if (archive_file == NULL) {
        return NULL;
    }

    // Ligne ABSOLUE (dans le fichier .arch entier) où commence le
    // contenu de CE fichier précis : le body démarre à
    // archive->body_start_line_in_file, et file_content_start_line_in_body
    // est relatif à ce début — exactement le calcul "25 + 14 - 1 = 38"
    // fait à la main plus tôt dans la conception.
    long target_absolute_line = archive->body_start_line_in_file
                               + file_node->file_content_start_line_in_body - 1;

    // On avance jusqu'à cette ligne en lisant (et en jetant) tout ce
    // qui précède — pas d'accès direct "ligne N" possible sur un
    // fichier texte, il faut le parcourir séquentiellement depuis le
    // début. Coûteux si répété souvent sur un gros fichier, mais
    // suffisant pour ce projet (pas d'exigence de performance dans le
    // sujet, et chaque appel est indépendant, pas de boucle cachée).
    char line_buffer[ARCHIVE_BODY_LINE_MAX_LENGTH];
    long current_line_number = 1;
    while (current_line_number < target_absolute_line) {
        if (read_one_line_from_archive_file(archive_file, line_buffer, sizeof(line_buffer)) < 0) {
            // Le fichier se termine avant d'atteindre la position que
            // le header promettait : archive corrompue ou
            // désynchronisée (ex: modifiée à la main entre-temps).
            fclose(archive_file);
            return NULL;
        }
        current_line_number++;
    }

    // Contrairement à parse_raw_header_blocks (où le NOMBRE de blocs
    // n'est pas connu à l'avance, d'où les realloc successifs), ici on
    // connaît DÉJÀ exactement le nombre de lignes à lire
    // (file_content_line_count) et la longueur maximale d'une ligne
    // (ARCHIVE_BODY_LINE_MAX_LENGTH) : un seul malloc, dimensionné en
    // une fois pour le pire cas possible, suffit — pas besoin de faire
    // grandir le buffer au fur et à mesure.
    size_t max_possible_content_size =
        (size_t)file_node->file_content_line_count * (ARCHIVE_BODY_LINE_MAX_LENGTH + 1) + 1;
    char *content = malloc(max_possible_content_size);
    if (content == NULL) {
        fclose(archive_file);
        return NULL;
    }
    size_t content_length = 0;

    for (long line_index = 0; line_index < file_node->file_content_line_count; line_index++) {
        int line_length = read_one_line_from_archive_file(archive_file, line_buffer, sizeof(line_buffer));
        if (line_length < 0) {
            // L'archive promettait plus de lignes que ce qu'elle
            // contient réellement à cet endroit.
            free(content);
            fclose(archive_file);
            return NULL;
        }

        // memcpy plutôt que strcpy/strcat : on connaît déjà line_length
        // exactement (retourné par read_one_line_from_archive_file), pas
        // besoin que memcpy aille elle-même chercher un '\0' pour savoir
        // où s'arrêter.
        memcpy(content + content_length, line_buffer, (size_t)line_length);
        content_length += (size_t)line_length;

        if (line_index + 1 < file_node->file_content_line_count) {
            // On réinsère un '\n' ENTRE les lignes — read_one_line_from_archive_file
            // le retire systématiquement à la lecture, donc sans ce
            // réajout le contenu de plusieurs lignes se retrouverait
            // toutes collées bout à bout. On ne le fait PAS après la
            // toute dernière ligne : on ne sait pas si le fichier
            // d'origine se terminait par un retour à la ligne ou non —
            // cette information est perdue par la nature même d'un
            // format qui compte en LIGNES plutôt qu'en octets exacts,
            // pas une approximation qu'on introduit nous-mêmes ici.
            content[content_length++] = '\n';
        }
    }
    content[content_length] = '\0';

    fclose(archive_file);
    return content;
}