// ============================================================
//  PASSE 1 : lecture BRUTE du header d'une archive, bloc par bloc,
//  SANS relier un sous-répertoire à son bloc correspondant (ça, c'est
//  le travail de archive_tree_builder.c). Voir le diagramme "Détail de
//  la passe 1" pour le déroulement complet.
// ============================================================

#include "archive_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_HEADER_LINE_MAX_LENGTH 1024
// Taille maximale, en octets, d'une ligne lue dans le header d'un
// fichier .arch. Interne à ce fichier : contrairement à
// MAX_PROTOCOL_LINE_LENGTH (dans protocol.h), ça ne concerne que la
// lecture de fichiers .arch sur le disque, pas le réseau — les deux
// constantes n'ont pas à être liées entre elles.


int read_one_line_from_archive_file(FILE *archive_file,
                                     char *line_content_buffer,
                                     size_t line_content_buffer_capacity)
{
    // fgets() lit au plus (capacity - 1) caractères, s'arrête au premier
    // '\n' rencontré (qu'il INCLUT dans le résultat), et termine toujours
    // le buffer par '\0'. Si fgets() retourne NULL, il n'y a plus rien à
    // lire : soit le fichier est fini (EOF), soit une erreur de lecture
    // est survenue — on traite les deux cas de la même façon ici (-1),
    // car aucun des deux ne permet de continuer la lecture de l'archive.
    if (fgets(line_content_buffer, (int)line_content_buffer_capacity, archive_file) == NULL) {
        return -1;
    }

    // strlen() donne la longueur de ce qui vient d'être lu, pour savoir
    // où se trouve (ou pas) le '\n' final.
    size_t line_length = strlen(line_content_buffer);

    if (line_length > 0 && line_content_buffer[line_length - 1] == '\n') {
        // Cas normal : la ligne tenait dans le buffer et se termine par
        // '\n'. On écrase ce '\n' par '\0' pour que le reste du code
        // manipule des chaînes "propres" — un strcmp(ligne, "@") devrait
        // échouer uniquement si la ligne n'est vraiment pas "@", pas à
        // cause d'un '\n' resté collé au bout.
        line_content_buffer[line_length - 1] = '\0';
        line_length--; // la longueur retournée ne doit pas compter ce '\n' qu'on vient d'enlever
    } else if (!feof(archive_file)) {
        // Cas rare : la VRAIE ligne du fichier est plus longue que le
        // buffer fourni — fgets() a rempli tout le buffer SANS jamais
        // atteindre de '\n', et ce n'est pas la fin du fichier (sinon
        // feof() serait vrai). Le reste de cette ligne existe encore
        // dans le fichier, non lu. Sans rien faire ici, le PROCHAIN
        // appel à cette fonction lirait la FIN de cette ligne-ci en
        // croyant lire une NOUVELLE ligne — un décalage silencieux de
        // tout le parsing à partir de ce point. On "vide" donc
        // manuellement les caractères restants jusqu'au '\n' (ou la fin
        // du fichier), un par un avec fgetc(), pour que l'appel suivant
        // reparte bien sur la ligne d'après. Même logique de
        // resynchronisation que receive_line() côté réseau, appliquée
        // ici à un fichier local plutôt qu'à un socket.
        int discarded_character;
        do {
            discarded_character = fgetc(archive_file);
        } while (discarded_character != '\n' && discarded_character != EOF);
    }

    // Cast (int) : line_length est un size_t (non signé), mais cette
    // fonction doit pouvoir retourner -1 pour signaler "plus rien à
    // lire" (voir plus haut) — un type non signé ne peut pas porter -1,
    // même raison que ssize_t pour send()/recv() dans network_io.c.
    return (int)line_length;
}


static int parse_archive_header_and_body_start_lines(FILE *archive_file,
                                                       long *out_header_start_line_number,
                                                       long *out_body_start_line_number)
{
    char first_line_buffer[ARCHIVE_HEADER_LINE_MAX_LENGTH];

    // On s'appuie sur read_one_line_from_archive_file plutôt que de
    // refaire une lecture ad hoc : même logique de découpage de ligne
    // partout dans ce fichier, un seul endroit à corriger si un jour on
    // change la façon de lire une ligne.
    if (read_one_line_from_archive_file(archive_file, first_line_buffer,
                                         sizeof(first_line_buffer)) < 0) {
        // Fichier vide : impossible de lire ne serait-ce que la ligne 1.
        return -1;
    }

    // sscanf() avec le motif "%ld:%ld" ne réussit à écrire DEUX valeurs
    // que si le texte correspond exactement au format "<entier>:<entier>"
    // (ex: "3:25"). Si la ligne 1 est mal formée (pas de ':', du texte à
    // la place de chiffres...), sscanf() renverra moins de 2 — on
    // détecte l'erreur via SA valeur de retour plutôt que de supposer
    // que le contenu est toujours valide.
    //
    // Ces deux valeurs ne servent pas qu'à savoir où s'arrêter de lire :
    // out_body_start_line_number sera conservé telle quelle jusque dans
    // Archive->body_start_line_in_file, et resservira plus tard pour
    // localiser le contenu d'un fichier (ex: cat) à partir de sa
    // position relative dans le body.
    if (sscanf(first_line_buffer, "%ld:%ld",
               out_header_start_line_number,
               out_body_start_line_number) != 2) {
        return -1;
    }

    return 0;
}


static int parse_raw_header_entry_line(const char *entry_line_text, RawHeaderEntry *out_entry)
{
    // Buffers LOCAUX de taille fixe pour recevoir le résultat brut de
    // sscanf(), avant de le recopier avec la bonne taille (via strdup)
    // dans out_entry. sscanf() ne peut pas écrire directement dans
    // out_entry->raw_entry_name : ce pointeur vaut NULL tant qu'on ne
    // l'a pas explicitement alloué, sscanf() a besoin d'un espace déjà
    // réservé pour écrire dedans.
    char entry_name_buffer[ARCHIVE_HEADER_LINE_MAX_LENGTH];
    char entry_permissions_buffer[12]; // 10 caractères de droits + '\0' + 1 de marge

    // Initialisés à 0 : si sscanf() ne remplit pas ces 2 derniers champs
    // (cas à 3 champs), ils garderont une valeur neutre et connue
    // plutôt qu'une valeur indéterminée restée sur la pile.
    long parsed_size_in_bytes = 0;
    long parsed_body_start_line = 0;
    long parsed_body_line_count = 0;

    // %1023s et %11s bornent explicitement le nombre de caractères que
    // sscanf() peut écrire dans chaque buffer (1023 = taille du buffer
    // moins 1, pour laisser la place au '\0' final que sscanf() ajoute
    // lui-même). Sans cette limite, une ligne plus longue que prévu (ou
    // une archive volontairement corrompue) pourrait écrire au-delà de
    // la fin d'un buffer alloué sur la pile — un dépassement de tampon
    // classique, rendu impossible ici par construction.
    //
    // La VALEUR DE RETOUR de sscanf() (pas une supposition sur la
    // taille) dit combien de champs ont réellement été reconnus : c'est
    // ce nombre, pas "raw_entry_size_in_bytes == 0", qui distingue une
    // entrée à 3 champs (répertoire ou fichier vide) d'une entrée à 5
    // champs (fichier non vide) — l'exemple du sujet contient justement
    // un fichier de taille 0 ("toto4 ... 0 17 0") qui a quand même ses
    // 2 derniers champs présents.
    int fields_successfully_parsed =
        sscanf(entry_line_text, "%1023s %11s %ld %ld %ld",
               entry_name_buffer,
               entry_permissions_buffer,
               &parsed_size_in_bytes,
               &parsed_body_start_line,
               &parsed_body_line_count);

    // D'après le format du sujet, une entrée valide a TOUJOURS 3 ou 5
    // champs — jamais 4, jamais moins de 3. Tout autre compte signale
    // une ligne mal formée.
    if (fields_successfully_parsed != 3 && fields_successfully_parsed != 5) {
        return -1;
    }

    // strdup() alloue exactement (strlen(entry_name_buffer) + 1) octets
    // et copie le nom dedans — contrairement au buffer fixe ci-dessus,
    // out_entry->raw_entry_name (un char* dans archive_internal.h)
    // n'occupera que la mémoire réellement nécessaire pour CE nom
    // précis, ni plus ni moins.
    out_entry->raw_entry_name = strdup(entry_name_buffer);
    if (out_entry->raw_entry_name == NULL) {
        // malloc (utilisé en interne par strdup) a échoué, mémoire
        // épuisée. On remonte l'erreur plutôt que de continuer avec un
        // pointeur NULL qui ferait planter le code plus tard, plus loin
        // du vrai endroit du problème.
        return -1;
    }

    // raw_entry_permissions est un TABLEAU de taille fixe (pas un
    // pointeur, voir archive_internal.h) : pas d'allocation nécessaire,
    // une simple copie suffit. strncpy avec 10 comme limite + le '\0'
    // posé manuellement garantit qu'on ne dépasse jamais les 11 octets
    // réservés, même si entry_permissions_buffer contenait par erreur
    // plus de 10 caractères utiles.
    strncpy(out_entry->raw_entry_permissions, entry_permissions_buffer, 10);
    out_entry->raw_entry_permissions[10] = '\0';

    out_entry->raw_entry_size_in_bytes = parsed_size_in_bytes;

    if (fields_successfully_parsed == 5) {
        // Fichier non vide : les 2 derniers champs localisent son
        // contenu dans le body (voir le diagramme du format du header :
        // ligne de départ RELATIVE au body, pas ligne absolue du fichier).
        out_entry->raw_entry_file_content_start_line_in_body = parsed_body_start_line;
        out_entry->raw_entry_file_content_line_count = parsed_body_line_count;
    } else {
        // Répertoire ou fichier vide : ces champs n'existaient pas sur
        // la ligne lue. On les met à 0 explicitement plutôt que de
        // laisser une valeur indéterminée dans out_entry, qui n'a pas
        // été initialisé avant cet appel.
        out_entry->raw_entry_file_content_start_line_in_body = 0;
        out_entry->raw_entry_file_content_line_count = 0;
    }

    return 0;
}


void free_raw_header_blocks(RawHeaderBlock *header_blocks_array,
                             size_t header_blocks_array_count)
{
    // Chaque RawHeaderBlock contient lui-même des pointeurs alloués
    // séparément (son chemin, son tableau d'entrées, et le nom de
    // chaque entrée) : libérer uniquement header_blocks_array avec un
    // seul free() laisserait fuir toute cette mémoire "de l'intérieur",
    // puisque ces blocs-là ne seraient plus jamais atteignables. Il
    // faut donc descendre dans CHAQUE niveau avant de remonter.
    for (size_t block_index = 0; block_index < header_blocks_array_count; block_index++) {
        RawHeaderBlock *current_block = &header_blocks_array[block_index];

        // Chemin du bloc, alloué par strdup() dans parse_raw_header_blocks.
        free(current_block->raw_block_directory_path);

        // Avant de libérer le tableau d'entrées lui-même, il faut
        // d'abord libérer le nom de CHAQUE entrée qu'il contient (lui
        // aussi alloué séparément par strdup() dans
        // parse_raw_header_entry_line) — sinon ces noms resteraient
        // orphelins en mémoire : plus aucun pointeur ne les désignerait
        // une fois le tableau libéré, donc plus aucun moyen de les
        // libérer à leur tour. C'est une vraie fuite mémoire.
        for (size_t entry_index = 0; entry_index < current_block->raw_block_entry_count; entry_index++) {
            free(current_block->raw_block_entries[entry_index].raw_entry_name);
        }
        // Le tableau d'entrées lui-même (un seul bloc mémoire contigu,
        // issu de realloc — pas un bloc par entrée).
        free(current_block->raw_block_entries);
    }
    // Enfin, le tableau de blocs lui-même, une fois que tout son
    // contenu interne a déjà été libéré.
    free(header_blocks_array);
}


static RawHeaderBlock *parse_raw_header_blocks(FILE *archive_file,
                                                long *current_line_number,
                                                long body_start_line_number,
                                                size_t *out_block_count)
{
    // NULL + 0 : représentation standard en C d'un tableau dynamique
    // "encore vide". realloc(NULL, taille) se comporte exactement comme
    // malloc(taille) au tout premier agrandissement — pas besoin
    // d'écrire un cas particulier pour "le tout premier bloc".
    RawHeaderBlock *header_blocks_array = NULL;
    size_t header_blocks_array_count = 0;

    char current_line_text[ARCHIVE_HEADER_LINE_MAX_LENGTH];

    // Constante locale : le préfixe qui identifie une ligne de début de
    // bloc. "static const" plutôt qu'un simple tableau local : elle
    // n'est calculée qu'une seule fois pour toute la durée du programme
    // (pas recréée à chaque appel de la fonction), et "const" documente
    // qu'elle n'est jamais modifiée.
    static const char DIRECTORY_LINE_PREFIX[] = "directory ";
    size_t directory_line_prefix_length = strlen(DIRECTORY_LINE_PREFIX);

    // Condition d'arrêt : on lit des lignes tant qu'on n'a pas atteint
    // la ligne où le body commence (donnée par la ligne 1 de l'archive,
    // "3:25" → body_start_line_number vaut 25).
    while (*current_line_number < body_start_line_number) {

        int line_length = read_one_line_from_archive_file(archive_file, current_line_text,
                                                            sizeof(current_line_text));
        if (line_length < 0) {
            // Le fichier s'arrête AVANT d'avoir atteint la ligne de
            // début du body annoncée en ligne 1 : soit l'archive est
            // tronquée, soit sa ligne 1 mentait sur sa propre
            // structure — dans les deux cas impossible de continuer. On
            // libère ce qui a déjà été construit jusqu'ici (pas de fuite
            // même sur un échec en cours de route) avant de signaler
            // l'erreur.
            free_raw_header_blocks(header_blocks_array, header_blocks_array_count);
            return NULL;
        }
        // Incrémenté APRÈS la lecture réussie : current_line_number doit
        // refléter la ligne qu'on vient de consommer, pas celle qu'on
        // s'apprête à lire au prochain tour de boucle.
        (*current_line_number)++;

        if (strncmp(current_line_text, DIRECTORY_LINE_PREFIX, directory_line_prefix_length) == 0) {
            // strncmp (pas strcmp) car on ne compare que le DÉBUT de la
            // ligne avec "directory " — le reste de la ligne est le
            // chemin, qui change à chaque bloc et ne doit pas entrer
            // dans la comparaison.

            // On agrandit le tableau de blocs d'UN élément à chaque
            // nouveau bloc rencontré. realloc() peut DÉPLACER tout le
            // bloc mémoire s'il ne peut pas l'étendre sur place — ça ne
            // pose aucun problème ici, car ce tableau ne contient que
            // des POINTEURS vers d'autres allocations
            // (raw_block_directory_path, raw_block_entries), jamais les
            // données elles-mêmes : déplacer le tableau ne déplace pas
            // ce vers quoi il pointe.
            RawHeaderBlock *grown_array = realloc(
                header_blocks_array,
                (header_blocks_array_count + 1) * sizeof(RawHeaderBlock));
            if (grown_array == NULL) {
                // Si realloc échoue, l'ANCIEN bloc (header_blocks_array)
                // reste intact et valide — c'est pourquoi on le passe
                // tel quel à free_raw_header_blocks, et surtout pas
                // grown_array qui vaut NULL dans ce cas.
                free_raw_header_blocks(header_blocks_array, header_blocks_array_count);
                return NULL;
            }
            header_blocks_array = grown_array;

            // &header_blocks_array[header_blocks_array_count] est
            // l'adresse du nouvel élément, juste après ceux déjà remplis
            // (index allant de 0 à header_blocks_array_count - 1).
            RawHeaderBlock *new_block = &header_blocks_array[header_blocks_array_count];

            // current_line_text + directory_line_prefix_length avance le
            // pointeur de 10 caractères (la longueur de "directory ")
            // pour ne garder que le chemin qui suit sur la ligne —
            // arithmétique de pointeur sur un char*, chaque unité
            // ajoutée avance d'exactement un octet.
            new_block->raw_block_directory_path =
                strdup(current_line_text + directory_line_prefix_length);
            new_block->raw_block_entries = NULL;   // pas encore d'entrées : tableau vide pour l'instant
            new_block->raw_block_entry_count = 0;

            header_blocks_array_count++;

        } else if (strcmp(current_line_text, "@") == 0) {
            // strcmp (égalité stricte, cette fois) : "@" seul sur sa
            // ligne marque la fin du bloc courant. Le bloc est déjà
            // entièrement enregistré (créé à sa ligne "directory",
            // rempli entrée par entrée depuis) : rien à construire ici,
            // "continue" passe directement à la lecture de la ligne
            // suivante.
            continue;

        } else {
            // Toute autre ligne est une ENTRÉE, qui appartient forcément
            // au DERNIER bloc ouvert (celui dont la ligne "directory" a
            // été vue le plus récemment et n'a pas encore été refermée
            // par un "@").
            if (header_blocks_array_count == 0) {
                // Une ligne d'entrée AVANT toute ligne "directory" :
                // l'archive ne respecte pas le format attendu (une
                // entrée doit toujours être précédée d'un bloc qui
                // l'accueille).
                free_raw_header_blocks(header_blocks_array, header_blocks_array_count);
                return NULL;
            }

            // "- 1" : header_blocks_array_count est le NOMBRE de blocs
            // déjà créés, donc l'index du DERNIER bloc valide est
            // toujours ce nombre moins un (indexation C à partir de 0).
            RawHeaderBlock *current_block = &header_blocks_array[header_blocks_array_count - 1];

            RawHeaderEntry parsed_entry;
            if (parse_raw_header_entry_line(current_line_text, &parsed_entry) < 0) {
                // La ligne ne correspond à aucun format d'entrée valide
                // (ni 3 ni 5 champs reconnus).
                free_raw_header_blocks(header_blocks_array, header_blocks_array_count);
                return NULL;
            }

            // Même logique de realloc que pour le tableau de blocs,
            // appliquée cette fois au tableau d'entrées DU BLOC COURANT
            // uniquement — chaque bloc a son propre tableau d'entrées,
            // indépendant de celui des autres blocs.
            RawHeaderEntry *grown_entries = realloc(
                current_block->raw_block_entries,
                (current_block->raw_block_entry_count + 1) * sizeof(RawHeaderEntry));
            if (grown_entries == NULL) {
                // parsed_entry.raw_entry_name a été alloué par
                // parse_raw_header_entry_line juste au-dessus et n'est
                // encore stocké NULLE PART dans header_blocks_array à ce
                // stade : si on ne le libère pas ICI avant de partir en
                // erreur, il fuit — free_raw_header_blocks ne le
                // trouvera jamais, puisqu'il n'est rattaché à aucun bloc
                // du tableau.
                free(parsed_entry.raw_entry_name);
                free_raw_header_blocks(header_blocks_array, header_blocks_array_count);
                return NULL;
            }
            current_block->raw_block_entries = grown_entries;
            // Copie de la STRUCTURE parsed_entry (tous ses champs, dont
            // le pointeur raw_entry_name) dans la case nouvellement
            // allouée du tableau — une copie SUPERFICIELLE du pointeur,
            // pas une copie profonde de la chaîne qu'il pointe, et c'est
            // volontaire : parsed_entry est une variable locale qui va
            // disparaître à la fin de cette itération de boucle, mais la
            // mémoire allouée par strdup() qu'elle référence continue
            // d'exister, maintenant accessible depuis le tableau.
            current_block->raw_block_entries[current_block->raw_block_entry_count] = parsed_entry;
            current_block->raw_block_entry_count++;
        }
    }

    *out_block_count = header_blocks_array_count;
    return header_blocks_array;
}


RawHeaderBlock *parse_raw_header_blocks_from_archive_file(
    const char *archive_file_path,
    long *out_body_start_line_number,
    size_t *out_block_count)
{
    // "r" : ouverture en lecture SEULE, en mode texte (pas "rb") — on
    // veut que le système traduise les fins de ligne selon la
    // plateforme si besoin, puisqu'on manipule un fichier texte, pas des
    // données binaires brutes.
    FILE *archive_file = fopen(archive_file_path, "r");
    if (archive_file == NULL) {
        // Fichier introuvable, droits insuffisants, chemin invalide...
        // fopen() ne distingue pas la cause précise ici, mais le
        // traitement qu'on en fait est le même dans tous les cas :
        // signaler l'échec à l'appelant.
        return NULL;
    }

    long header_start_line_number = 0;
    long body_start_line_number = 0;

    if (parse_archive_header_and_body_start_lines(archive_file,
            &header_start_line_number, &body_start_line_number) < 0) {
        // Ne pas oublier fclose() même sur un chemin d'erreur : sinon le
        // descripteur de fichier resterait ouvert inutilement — une
        // fuite de RESSOURCE SYSTÈME, différente d'une fuite mémoire
        // mais tout aussi réelle (un programme qui tourne longtemps
        // finirait par épuiser les descripteurs disponibles).
        fclose(archive_file);
        return NULL;
    }

    // La ligne 1 vient d'être consommée par l'appel ci-dessus : la
    // prochaine ligne à lire dans le fichier est donc la ligne 2.
    long current_line_number = 2;

    // On AVANCE dans le fichier jusqu'à la ligne où le header commence
    // réellement (header_start_line_number), en lisant et en IGNORANT
    // chaque ligne entre la ligne 2 et cette ligne-là (ex: la ligne 2
    // vide de l'exemple du sujet). skipped_line_buffer n'est utilisé que
    // comme destination jetable pour ces lectures : son contenu n'est
    // jamais exploité ensuite.
    char skipped_line_buffer[ARCHIVE_HEADER_LINE_MAX_LENGTH];
    while (current_line_number < header_start_line_number) {
        if (read_one_line_from_archive_file(archive_file, skipped_line_buffer,
                                             sizeof(skipped_line_buffer)) < 0) {
            // Le fichier se termine avant même d'atteindre le header
            // annoncé par sa propre ligne 1 : archive invalide.
            fclose(archive_file);
            return NULL;
        }
        current_line_number++;
    }

    // Délègue la vraie boucle de lecture des blocs à la fonction dédiée,
    // en lui passant current_line_number PAR ADRESSE (&current_line_number) :
    // parse_raw_header_blocks continue à l'incrémenter à partir d'où on
    // s'est arrêté ici, et doit pouvoir modifier la variable de
    // l'appelant pour que ce comptage reste cohérent d'un appel à
    // l'autre.
    size_t block_count = 0;
    RawHeaderBlock *blocks = parse_raw_header_blocks(archive_file, &current_line_number,
                                                      body_start_line_number, &block_count);
    // Le fichier n'est plus nécessaire après cet appel, qu'il ait
    // réussi ou échoué : on le ferme dans tous les cas avant de
    // retourner, plutôt que de dupliquer un fclose() dans chaque branche
    // qui suit.
    fclose(archive_file);

    if (blocks == NULL) {
        return NULL;
    }

    // On ne remplit les paramètres de sortie qu'une fois TOUT le
    // parsing réussi — jamais de valeurs partielles ou incohérentes
    // communiquées à l'appelant en cas d'échec en cours de route.
    *out_body_start_line_number = body_start_line_number;
    *out_block_count = block_count;
    return blocks;
}


#ifdef ARCHIVE_RAW_PARSER_TEST_MAIN
// Macro DIFFÉRENTE de celle d'archive.c (ARCHIVE_TEST_MAIN) : celle-ci
// permet de tester CE FICHIER SEUL, sans compiler archive_tree_builder.c
// ni archive.c — utile pour isoler un bug à la seule passe 1 sans faire
// intervenir le reste du programme.
static void print_raw_header_blocks_for_debugging(const RawHeaderBlock *header_blocks_array,
                                                    size_t header_blocks_array_count)
{
    for (size_t block_index = 0; block_index < header_blocks_array_count; block_index++) {
        const RawHeaderBlock *current_block = &header_blocks_array[block_index];
        printf("Bloc %zu : directory %s\n", block_index, current_block->raw_block_directory_path);

        for (size_t entry_index = 0; entry_index < current_block->raw_block_entry_count; entry_index++) {
            const RawHeaderEntry *current_entry = &current_block->raw_block_entries[entry_index];
            printf("  - %s %s %ld",
                   current_entry->raw_entry_name,
                   current_entry->raw_entry_permissions,
                   current_entry->raw_entry_size_in_bytes);
            if (current_entry->raw_entry_file_content_line_count > 0 ||
                current_entry->raw_entry_file_content_start_line_in_body > 0) {
                printf(" (body: ligne %ld, %ld ligne(s))",
                       current_entry->raw_entry_file_content_start_line_in_body,
                       current_entry->raw_entry_file_content_line_count);
            }
            printf("\n");
        }
    }
}

int main(int argument_count, char *argument_values[])
{
    if (argument_count != 2) {
        fprintf(stderr, "Usage: %s <chemin_archive>\n", argument_values[0]);
        return 1;
    }

    long body_start_line_number = 0;
    size_t block_count = 0;
    RawHeaderBlock *blocks = parse_raw_header_blocks_from_archive_file(
        argument_values[1], &body_start_line_number, &block_count);

    if (blocks == NULL) {
        fprintf(stderr, "Erreur : impossible de parser %s\n", argument_values[1]);
        return 1;
    }

    printf("body_start_line_number = %ld\n\n", body_start_line_number);
    print_raw_header_blocks_for_debugging(blocks, block_count);

    free_raw_header_blocks(blocks, block_count);
    return 0;
}
#endif