# LO14 — Réseau virtuel de machines (`rvsh`), 2026–2027

Projet Bash de Jules et Selyan. Le [sujet de référence](docs/LO14-Projet-2026-2027.pdf)
est conservé dans le dépôt. Le programme simule des machines, comptes, droits,
sessions et messages dans des fichiers locaux ; il ne crée aucun compte Linux.

L'ancien serveur d'archives `vsh` en C reste intact sur `archive/vsh-2025-2026`,
au commit `2a93ae5616736f44fddcdc6c077420f8f71e81a0`. Le développement actuel
se trouve sur `codex/rvsh-2026-2027`, avec le même historique de contributions.

## Prérequis

Linux, Bash 3.2 ou version suivante, OpenSSL avec `passwd -6`, `flock` (util-linux),
et les commandes usuelles `grep`, `awk`, `tr`, `date`, `sleep`, `mkdir`, `mv`, `rm`.
Aucun compilateur C ni serveur TCP requis. `make` et Python 3 servent uniquement
aux vérifications. macOS ne fournit pas `flock` par défaut : utiliser Linux/WSL
ou les tests Docker décrits plus bas.

## Première utilisation

Depuis le dossier du projet :

```bash
./rvsh -admin
```

Le premier lancement demande de **définir et confirmer** le mot de passe de root.
Les lancements suivants demandent ce mot de passe pour ouvrir `root@hostroot >`.
Aucun mot de passe n'est fourni ou enregistré dans le dépôt.

Exemple dans le shell administrateur :

```text
host add machineA
host add machineB
user add selyan
user grant selyan machineA
user grant selyan machineB
afinger selyan Membre du binôme LO14
rhost
user list
```

`user add` demande le nouveau mot de passe et sa confirmation. Ouvrir ensuite
**un deuxième terminal sur le même ordinateur et la même copie du projet** :

```bash
./rvsh -connect machineA selyan
```

Après vérification du mot de passe et des droits, le prompt devient `selyan@machineA >`.
Dans cette session :

```text
who
rusers
finger
rconnect machineB
exit
exit
```

Le premier `exit` revient à machineA ; le second ferme la session. `help` affiche
les commandes. Les chemins contenant des espaces sont acceptés pour le projet
et le stockage ; les noms de comptes et machines sont limités à 32 caractères,
commencent par une lettre et acceptent lettres ASCII, chiffres, `_` et `-`.

## Commandes disponibles

| Commande | Fonctionnement |
|---|---|
| `who` | Sessions sur la machine courante, avec date et heure. |
| `rusers` | Sessions sur toutes les machines, avec date et heure. |
| `rhost` | Liste des machines virtuelles. |
| `rconnect machine` | Nouvelle connexion avec l'utilisateur courant, sous réserve des droits. |
| `su utilisateur` | Authentification d'un autre utilisateur sur la même machine. |
| `passwd` | Vérification du mot de passe actuel et remplacement pour tout le réseau. |
| `finger [utilisateur]` | Informations complémentaires ; utilisateur courant par défaut. |
| `write utilisateur@machine message` | Message à toutes les sessions correspondantes. |
| `exit` | Ferme la connexion courante et revient à la précédente, le cas échéant. |
| `help` | Aide interactive. |

Commandes réservées à root dans une session issue de `-admin` :

| Commande | Fonctionnement |
|---|---|
| `host add machine` / `host del machine` / `host list` | Gestion des machines. |
| `user add utilisateur` / `user del utilisateur` / `user list` | Gestion des comptes. |
| `user grant utilisateur machine` / `user revoke utilisateur machine` | Gestion des droits. |
| `user passwd utilisateur` | Redéfinition du mot de passe d'un compte ordinaire. |
| `afinger utilisateur informations` | Mise à jour des informations complémentaires. |
| `wall message` | Diffusion à toutes les sessions connectées. |
| `wall -n message` | Diffusion immédiate, plus livraison à la prochaine connexion pour les comptes hors ligne. |

Les mots de passe sont saisis sans affichage et stockés sous forme de hachages
SHA-512 crypt salés. Les messages peuvent contenir des espaces. Les noms ne
nécessitent pas de guillemets ; ce shell virtuel n'interprète ni expansions,
ni pipelines, ni commandes du système hôte.

## Données, sessions et règles retenues

Les données sont conservées dans `data/`, ignoré par Git. Pour choisir un autre
réseau indépendant :

```bash
RVSH_DATA_DIR=/chemin/vers/mon-reseau ./rvsh -admin
```

Tous les terminaux d'un même réseau doivent utiliser le même dossier de données.
Un dossier existant non vide et sans marqueur rvsh est refusé. Le dossier est
privé (mode 700), et les fichiers créés sont privés (mode 600).

Les accès partagés sont protégés par `flock`. Plusieurs connexions d'un même
compte sur une machine restent distinctes. Les connexions précédentes d'une
chaîne A → B → C restent enregistrées comme connectées jusqu'à leur `exit`.
La suppression d'un compte ou d'une machine utilisée et le retrait d'un droit
utilisé sont refusés. `hostroot` et le compte root sont protégés.

`su` remplace l'identité de la connexion courante sans changer de machine.
Un compte ordinaire entre dans le mode admin uniquement par `rvsh -admin`.
Une session admin passée à un compte ordinaire perd les commandes admin tant
qu'elle n'est pas revenue à root après authentification.

Les messages sont affichés au plus tôt par un processus de notification
(vérification environ toutes les 0,2 seconde), même pendant l'attente d'une
commande. L'affichage peut couper visuellement la ligne en cours de saisie ;
la ligne saisie reste traitée par le lecteur Bash. Un `wall -n` différé est
consommé à la première reconnexion du compte et n'est pas répété ensuite.

Une sortie normale, EOF, SIGINT, SIGHUP ou SIGTERM nettoie les sessions du
processus. Après SIGKILL, les enregistrements orphelins sont retirés lors d'une
consultation ou d'un envoi de message. L'identification repose sur les PID
locaux : ce projet est une simulation pour plusieurs terminaux sur un hôte,
pas un dispositif d'isolation entre utilisateurs du système d'exploitation.

## Vérifications

```bash
make test    # syntaxe Bash, 11 cas CLI et 7 scénarios d'intégration multi-terminaux
make lint    # nécessite ShellCheck
```

Les tests utilisent uniquement des dossiers et comptes temporaires. Ils ne
créent pas de données dans le dossier `data/` du projet.

Pour vérifier sous Linux depuis une machine disposant de Docker :

```bash
docker build -t rvsh-tests:local -f tests/Dockerfile .
docker run --rm --network none --read-only --tmpfs /tmp:rw,nosuid,nodev \
  -v "$PWD:/project:ro" -w /project rvsh-tests:local make test
```

## Essayer sur macOS avec Docker

Après la construction de l'image ci-dessus, démarrer **un seul conteneur** pour
que les terminaux partagent aussi les identifiants de processus :

```bash
docker run --rm -d --name rvsh-demo --network none --read-only \
  --tmpfs /tmp:rw,nosuid,nodev -v "$PWD:/project:ro" \
  -v rvsh-demo-data:/rvsh-data -e RVSH_DATA_DIR=/rvsh-data \
  rvsh-tests:local sleep infinity
```

Premier terminal : `docker exec -it rvsh-demo ./rvsh -admin`.
Deuxième terminal, après création du compte et de ses droits :
`docker exec -it rvsh-demo ./rvsh -connect machineA selyan`.
Les données restent dans le volume local Docker `rvsh-demo-data`.
Après avoir quitté les sessions, arrêter la démonstration avec
`docker stop rvsh-demo`. Ne pas lancer un conteneur différent par terminal.

## Organisation et livrables

- `rvsh` : interface de lancement et authentification initiale.
- `lib/storage.sh` : données, mots de passe et verrouillage.
- `lib/session.sh` : sessions imbriquées et réception des messages.
- `lib/commands.sh` : commandes du shell virtuel.
- `tests/` : tests CLI, tests fonctionnels par pseudo-terminaux et environnement Linux.
- [Cahier des charges](docs/CAHIER-DES-CHARGES.md) : exigences et choix de comportement.
- [Guide de reprise](docs/REPRISE.md) : branches et publication manuelle.
- [Rapport Codex](RAPPORT-CODEX.md) : preuves, limites et actions restantes.

Rapports demandés avant le 4 décembre 2026 puis le 10 janvier 2027.
Présentation et test annoncés pendant la première semaine de janvier 2027 ;
créneau exact à confirmer avec l'enseignant.
