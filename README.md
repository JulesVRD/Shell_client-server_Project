# LO14 — Serveur d'archives (`vsh`)

Projet LO14 (UTT, 2025-2026) : un serveur d'archives interrogeable via une commande shell `vsh` fonctionnant selon 4 modes (`-list`, `-create`, `-browse`, `-extract`). Voir l'énoncé complet du sujet pour la spécification exacte du format d'archive.

## Prérequis

- Linux (testé sous Kali via WSL)
- `gcc`, `make`, `gdb`
- VS Code avec les extensions **C/C++** (`ms-vscode.cpptools`) et **Makefile Tools**, installées côté **WSL: kali-linux** si tu utilises Remote-WSL (pas seulement en local)

## Architecture

Deux exécutables **totalement indépendants**, qui ne communiquent que par socket TCP — ni l'un ni l'autre n'a besoin du code source de l'autre pour fonctionner :

- **`server`** : écoute sur un port, héberge un dossier de fichiers `.arch`, répond aux commandes reçues.
- **`vsh`** (client) : se connecte à un serveur (IP ou nom d'hôte + port), envoie une commande selon le mode demandé, affiche la réponse.

Une **archive** représente une arborescence de fichiers/répertoires sous forme d'un fichier texte à deux parties (voir §1.1 du sujet) :
- **header** : liste des répertoires et, pour chacun, les fichiers/sous-répertoires qu'il contient (nom, droits d'accès, taille, et pour un fichier non vide : position et longueur de son contenu dans le body).
- **body** : le contenu brut de tous les fichiers non vides décrits dans le header.

## Arborescence du projet

```
LO14-Project/
├── Makefile
├── .gitignore
├── README.md
├── .vscode/
│   ├── launch.json
│   └── tasks.json
├── include/
│   ├── protocol.h       # vocabulaire du protocole (commandes, codes de réponse)
│   ├── network_io.h      # transport réseau fiable (send/receive sur TCP)
│   └── archive.h         # API publique du modèle d'archive (ArchiveNode, Archive, etc.)
├── src/
│   ├── common/            # compilé dans vsh ET server — logique indépendante du réseau
│   │   ├── network_io.c            # implémentation du transport fiable
│   │   ├── archive.c                # API publique : archive_parse / archive_free
│   │   ├── archive_internal.h       # déclarations privées partagées entre les fichiers ci-dessous
│   │   ├── archive_raw_parser.c     # passe 1 : lecture brute du header
│   │   ├── archive_tree_builder.c   # passe 2 : construction de l'arbre ArchiveNode
│   │   ├── archive_navigation.c     # pwd, ls, résolution de chemin (pour cd)
│   │   ├── archive_body.c           # lecture du contenu d'un fichier (pour cat)
│   │   ├── archive_mutation.c       # touch, mkdir, rm (modifient l'arbre + sauvegardent)
│   │   └── archive_persistence.c    # réécriture complète de l'archive sur le disque
│   ├── server/
│   │   ├── server.c          # squelette réseau (socket/bind/listen/accept) + aiguillage
│   │   ├── server_internal.h # déclarations partagées entre les fichiers server_xxx.c
│   │   └── server_list.c     # implémentation du mode LIST
│   └── client/
│       ├── vsh.c              # parsing argv, connexion au serveur, aiguillage
│       ├── client_internal.h  # déclarations partagées entre les fichiers client_xxx.c
│       └── client_list.c      # implémentation du mode -list
├── obj/                    # généré par make — non versionné
├── bin/                    # exécutables générés — non versionné
└── test_archives/
    └── example.arch        # l'exemple du §1.1 du sujet, avec un body complet
```

## Compilation

```bash
make          # compile tout : vsh et server
make clean    # supprime obj/ et bin/
```

Le `Makefile` compile automatiquement tout fichier `.c` trouvé dans `src/common/`, `src/client/` et `src/server/` — ajouter un nouveau fichier dans un de ces dossiers (ex: `server_browse.c`) ne demande aucune modification du `Makefile`.

## Lancer une session de test

Le serveur tourne **indéfiniment** une fois lancé (il n'y a pas besoin de le relancer entre deux tests, seulement après une recompilation ou un arrêt volontaire) :

```bash
mkdir -p server_archives
cp test_archives/example.arch server_archives/        # au moins une vraie archive pour tester

./server 9000 server_archives &    # & pour le laisser tourner en arrière-plan
./vsh -list 127.0.0.1 9000         # ou avec un nom d'hôte : ./vsh -list localhost 9000
```

Pour un test entre deux machines (toi et ton binôme, chacun sur son poste) : celui qui lance `./server` donne sa **vraie adresse IP** sur le réseau partagé (école ou VM) à l'autre, qui l'utilise à la place de `127.0.0.1`. Aucune modification de code n'est nécessaire — `vsh` résout l'adresse via `getaddrinfo()`, qui accepte aussi bien une IP littérale qu'un nom d'hôte.

Le client n'a besoin ni du code source du serveur, ni d'accès aux fichiers `.arch` du serveur : tout ce qu'il sait de l'archive passe exclusivement par le réseau.

## Débogage dans VS Code

`.vscode/launch.json` contient un profil `cppdbg` par binaire/mode testé (ex. "Debug: server", "Debug: vsh -list"). Chaque profil déclenche automatiquement `make` avant de lancer (`preLaunchTask: build`, défini dans `tasks.json`).

## Protocole applicatif (`include/protocol.h`)

| Mode `vsh` | Échange | État |
|---|---|---|
| `-list` | client: `LIST` → serveur: `OK`, nombre d'archives, puis une ligne par archive | **Fait, testé de bout en bout** |
| `-browse` | client: `BROWSE <nom>` → serveur: `OK` → boucle de sous-commandes (`pwd`, `ls`, `cd`, `cat`, `rm`, `touch`, `mkdir`, `exit`) | Logique complète dans `common/`, pas encore branchée sur le réseau |
| `-create` | client: `CREATE <nom>` → serveur: `OK` → client: taille puis contenu de l'archive | Pas commencé (probablement côté client, lecture du vrai système de fichiers) |
| `-extract` | client: `EXTRACT <nom>` → serveur: taille puis contenu de l'archive | Pas commencé (probablement côté client, écriture sur le vrai système de fichiers) |

## État d'avancement

**Fait et testé (valgrind : 0 fuite sur tout ce qui suit) :**
- Transport réseau fiable (`network_io.c`) : envoi/réception exacts sur TCP, gestion `SIGPIPE`, lignes tronquées.
- Parsing complet d'une archive (`archive_raw_parser.c` + `archive_tree_builder.c`) : header → arbre en mémoire.
- Navigation en lecture (`archive_navigation.c`) : `pwd`, `ls` (court et `-l`), résolution de chemin absolu/relatif pour `cd`.
- Lecture du contenu d'un fichier (`archive_body.c`), pour `cat`.
- Modification de l'arbre + persistance réelle sur le disque (`archive_mutation.c` + `archive_persistence.c`) : `touch`/`mkdir`/`rm` réécrivent vraiment le fichier `.arch`, vérifié par reparsing complet après coup.
- Mode `-list` **de bout en bout** : `vsh -list` ↔ `server`, testé avec dossier vide, dossier introuvable, et plusieurs requêtes successives.

**À faire :**
- `server_browse.c` + `client_browse.c` : brancher toute la logique déjà écrite dans `common/` (navigation, mutation, lecture de contenu) sur une vraie session réseau qui reste ouverte.
- `-create` / `-extract` : lecture/écriture du vrai système de fichiers côté client (`opendir`/`readdir`/`stat`/`chmod`), probablement dans `src/client/`, pas dans `common/`.
- Gestion de plusieurs clients simultanés côté serveur (actuellement séquentiel, un client à la fois — suffisant pour les tests mais à revoir si plusieurs sessions `-browse` doivent coexister).
- Rapport intermédiaire et final (voir dates dans l'énoncé).

## Mettre à jour son dépôt local

```bash
git pull origin main
make clean && make   # recompiler après avoir récupéré les changements
```

## Auteurs

Binôme LO14 — Jules / Selyan)