# LO14 — Serveur d'archives (`vsh`)

Projet LO14 (UTT, 2025-2026) : un serveur d'archives interrogeable via une commande shell `vsh` fonctionnant selon 4 modes (`-list`, `-create`, `-browse`, `-extract`). Voir l'énoncé complet dans le PDF fourni par Florent Retraint pour la spécification exacte du format d'archive.

## Prérequis

- Linux (testé sous Kali via WSL)
- `gcc`, `make`, `gdb`
- VS Code avec les extensions **C/C++** (`ms-vscode.cpptools`) et **Makefile Tools**, installées côté **WSL: kali-linux** (pas seulement en local — voir note plus bas)

> **Piège rencontré et corrigé** : en environnement Remote-WSL, les extensions doivent être installées séparément côté WSL (section "WSL: kali-linux - Installed" dans le panneau Extensions), pas seulement côté Windows local. L'extension "C/C++ DevTools" seule ne suffit pas : il faut en plus la véritable extension **"C/C++"** de Microsoft (id `ms-vscode.cpptools`), qui fournit le débogueur `cppdbg` et valide `launch.json`.

## Architecture

Le projet comporte deux exécutables qui communiquent en TCP :

- **`server`** : écoute sur un port, reçoit les commandes du client, répond.
- **`vsh`** (client) : se connecte au serveur, envoie des commandes selon le mode demandé.

Une **archive** représente une arborescence de fichiers/répertoires sous forme d'un fichier texte à deux parties (voir §1.1 de l'énoncé) :
- **header** : liste des répertoires et, pour chaque répertoire, les fichiers/sous-répertoires qu'il contient (nom, droits d'accès, taille, et pour les fichiers non vides : ligne de début + nombre de lignes dans le body).
- **body** : le contenu brut de tous les fichiers non vides décrits dans le header.

## Arborescence du projet

```
LO14-Project/
├── Makefile
├── .gitignore
├── README.md
├── .vscode/
│   ├── launch.json       # profils de debug (un par mode : server, vsh -list, ...)
│   └── tasks.json        # tâche "build" (make all), appelée avant chaque debug
├── include/
│   ├── protocol.h         # vocabulaire du protocole (commandes, codes de réponse)
│   └── network_io.h       # contrat des fonctions d'envoi/réception réseau
├── src/
│   ├── common/
│   │   └── network_io.c   # implémentation des fonctions réseau (voir plus bas)
│   ├── server/             # (à venir) boucle socket + traitement des requêtes
│   └── client/             # (à venir) vsh.c : parsing argv, connexion, mode browse interactif
├── obj/                    # généré par make — non versionné
├── bin/                    # exécutables générés (vsh, server) — non versionné
└── test_archives/          # fichiers "arch" de test (ex. celui du §1.1 de l'énoncé)
```

## Compilation

```bash
make          # compile src/common, src/client, src/server -> bin/vsh et bin/server
make clean    # supprime obj/ et bin/
```

Le `Makefile` compile automatiquement tout fichier `.c` présent dans `src/common/`, `src/client/` et `src/server/` (via `wildcard`) — pas besoin de modifier le Makefile en ajoutant un nouveau fichier source dans un de ces dossiers.

## Débogage dans VS Code

`.vscode/launch.json` contient un profil `cppdbg` par binaire/mode testé (ex. "Debug: server", "Debug: vsh -list"). Chaque profil déclenche automatiquement `make` avant de lancer (`preLaunchTask: build`, défini dans `tasks.json`). Pour tester un autre mode (`-create`, `-browse`, `-extract`), dupliquer un bloc existant dans `launch.json` en changeant `name` et `args`.

Pour tester client et serveur en même temps, lancer deux sessions de debug séparées (une par profil) — VS Code le permet nativement.

## Protocole applicatif (`include/protocol.h`)

Ce header définit le "contrat" entre client et serveur — il doit être recompilé des deux côtés si on le modifie. Les commandes courtes passent en texte, terminées par `\n` ; les gros transferts (contenu complet d'une archive) passent par un préfixe de taille en octets suivi des données brutes.

| Mode `vsh` | Échange prévu |
|---|---|
| `-list` | client: `LIST` → serveur: nombre d'archives, puis une ligne par archive |
| `-create` | client: `CREATE <nom>` → serveur: `OK` → client: `<taille_octets>` puis le contenu de l'archive |
| `-browse` | client: `BROWSE <nom>` → serveur: `OK` → boucle de sous-commandes (`pwd`, `ls`, `cd`, `cat`, `rm`, `touch`, `mkdir`, `exit`) |
| `-extract` | client: `EXTRACT <nom>` → serveur: `<taille_octets>` puis le contenu de l'archive |

Codes de réponse génériques en première ligne de chaque réponse serveur : `OK` / `ERR`.

## Module réseau (`include/network_io.h` + `src/common/network_io.c`)

Ces fonctions résolvent un problème de base de la programmation socket : TCP est un flux continu d'octets, pas une suite de messages délimités — un `send()`/`recv()` peut transférer moins que demandé en un seul appel. Toutes les fonctions ci-dessous **bouclent** jusqu'à avoir transféré exactement ce qu'on attend.

| Fonction | Rôle | Retour |
|---|---|---|
| `send_all_bytes(fd, data, length)` | Envoie exactement `length` octets, quels qu'ils soient (texte ou binaire) | `0` OK, `-1` erreur |
| `receive_exact_bytes(fd, buffer, byte_count)` | Reçoit exactement `byte_count` octets (taille connue à l'avance — ex. transfert d'archive) | `0` OK, `-1` erreur/connexion fermée |
| `receive_line(fd, line_buffer, buffer_capacity)` | Reçoit une ligne de commande jusqu'au `\n` (taille inconnue à l'avance) | longueur lue si OK, `-1` connexion fermée, `-2` ligne tronquée (voir note) |
| `send_line(fd, line)` | Envoie `line` + `\n` | `0` OK, `-1` erreur |

### Deux points d'implémentation non évidents (important pour la suite du code)

1. **`receive_line` et les lignes trop longues (code retour `-2`)** — Si une ligne reçue dépasse `buffer_capacity`, la fonction ne s'arrête pas au milieu : elle continue à lire (en jetant le surplus) jusqu'au `\n` pour ne pas désynchroniser le flux réseau pour les lectures suivantes. Elle renvoie alors `-2` plutôt que `-1`, pour distinguer "ligne tronquée mais flux resynchronisé" de "connexion perdue". **Tout code appelant `receive_line` devra traiter ces deux cas d'erreur séparément**, pas avec un simple `if (resultat < 0)`.
2. **`MSG_NOSIGNAL` dans `send_all_bytes`** — Sur Linux, envoyer sur un socket dont le pair a déjà fermé la connexion déclenche le signal `SIGPIPE`, qui tue le processus entier par défaut (pas de retour `-1` propre). `MSG_NOSIGNAL` désactive ce comportement : l'erreur remonte normalement via la valeur de retour (`errno == EPIPE`). Sans ça, un seul client qui ferme sa connexion au mauvais moment pourrait planter tout le serveur, y compris les autres clients connectés.

## État d'avancement

**Fait :**
- Structure du projet, Makefile, config VS Code (build + debug)
- `protocol.h` (vocabulaire des commandes)
- `network_io.h/.c` (transport réseau fiable, avec gestion SIGPIPE et lignes tronquées)

**À faire :**
- `archive.c` (`src/common/`) : parsing et génération du format header/body décrit au §1.1
- `src/server/server.c` : `socket()`/`bind()`/`listen()`/`accept()`, boucle de traitement des 4 commandes
- `src/client/vsh.c` : parsing de `argv`, connexion au serveur, implémentation du mode `-browse` interactif (`pwd`, `ls`, `cd`, `cat`, `rm`, `touch`, `mkdir`)
- Gestion des droits d'accès lors de la restauration (mode `-extract`)
- Rapport intermédiaire (voir dates dans l'énoncé)

## Mettre à jour son dépôt local

```bash
git pull origin main
make clean && make   # recompiler après avoir récupéré les changements
```

## Auteurs

Binôme LO14 — Jules Ouvrard (et son binôme, à compléter)
