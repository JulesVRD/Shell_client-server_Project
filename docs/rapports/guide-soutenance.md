# Guide de soutenance

## Présentation et démonstration de rvsh

**LO14 - Jules et Selyan - version de travail du 8 novembre 2026**

Support à répéter ensemble. Durée cible : 20 minutes. Les commandes ci-dessous sont à exécuter dans un réseau de démonstration distinct de vos données de travail. Ce guide décrit un scénario ; sa présence ne vaut pas preuve d'une répétition humaine.

## 1. Déroulé oral

| Temps | Contenu | Point à faire comprendre |
|---|---|---|
| 0 à 2 min | Sujet et périmètre | Machines simulées, deux modes, plusieurs terminaux locaux. |
| 2 à 5 min | Structure et stockage | Lanceur, trois modules, fichiers et verrou partagé. |
| 5 à 14 min | Démonstration | Droits, sessions, connexions imbriquées et messages. |
| 14 à 17 min | Tests et défaut corrigé | Pseudo-terminaux et identité des processus. |
| 17 à 20 min | Limites et questions | Affichage du prompt, panne, rôle du système Linux. |

Une répartition possible est de faire expliquer l'architecture par l'un pendant que l'autre pilote la démonstration, puis d'inverser les rôles pour les tests et les questions. Choisir ensemble la répartition réelle, sans attribuer a posteriori des contributions non réalisées.

## 2. Préparation avant l'arrivée du jury

- Utiliser Linux/WSL ou le conteneur partagé décrit dans le README.
- Choisir un dossier neuf pour la démonstration. Ne pas effacer un dossier de travail pour repartir à zéro.
- Ouvrir deux terminaux dans la même copie et avec la même valeur de `RVSH_DATA_DIR`.
- Garder le README et le code des modules accessibles.
- Choisir des mots de passe de démonstration distincts de vos comptes réels ; les saisir uniquement aux invites.
- Exécuter `make test` et `make lint` avant la répétition. Conserver leur sortie si vous la présentez comme une preuve.

Sous Linux, définir dans les deux terminaux un même chemin vers un dossier neuf, par exemple un nom que vous n'avez pas encore utilisé :

```bash
export RVSH_DATA_DIR="$PWD/data/demo-repetition-01"
```

<!-- pagebreak -->

## 3. Administration et droits - terminal 1

```bash
./rvsh -admin
```

Définir le mot de passe root, puis saisir les commandes une par une. Les deux créations de compte demandent chacune un mot de passe et sa confirmation.

```text
host add machineA
host add machineB
host add machineC
user add alice
user add bob
user grant alice machineA
user grant alice machineB
user grant alice machineC
user grant bob machineA
afinger alice Membre du binôme LO14
host list
user list
```

Expliquer que créer un compte ne donne aucun accès implicite. Les droits sont attribués explicitement. Le compte root et `hostroot` sont protégés.

## 4. Connexions et identité - terminal 2

```bash
./rvsh -connect machineA alice
```

Saisir le mot de passe d'alice puis exécuter :

```text
who
rusers
finger
rconnect machineB
rconnect machineC
exit
exit
host add interdit
su bob
```

Les prompts doivent passer de A à B puis C, revenir sur B puis A, et rester sur A après `su bob` avec le mot de passe de bob. La commande `host add interdit` doit être refusée pour alice. Expliquer la pile de connexions et la différence entre changement de machine et changement d'utilisateur.

<!-- pagebreak -->

## 5. Messages immédiats et différés

Pendant que bob est connecté sur machineA, saisir dans le terminal administrateur :

```text
write bob@machineA Bonjour avec plusieurs mots
wall Message pour toutes les sessions
```

Montrer l'arrivée des messages dans le terminal 2 sans saisir une nouvelle commande. Fermer ensuite la session de bob avec `exit` dans le terminal 2.

Dans le terminal administrateur :

```text
wall -n Message pour la prochaine connexion
```

Relancer `./rvsh -connect machineA bob` dans le terminal 2. Le message différé doit être affiché à cette première reconnexion. Après une nouvelle sortie puis connexion, il ne doit plus être livré.

## 6. Mot de passe et protection des ressources

Dans la session de bob, saisir `passwd`, le mot de passe actuel puis un nouveau mot de passe confirmé. Sortir et se reconnecter pour vérifier la nouvelle valeur. Ne pas projeter ou écrire les mots de passe dans un document.

Pendant que bob est connecté, tester dans le terminal administrateur :

```text
user del bob
host del machineA
user revoke bob machineA
```

Les opérations doivent être refusées car la ressource ou le droit est utilisé. Après la sortie de bob, les opérations correspondantes deviennent possibles. Ne supprimer la machine qu'après avoir terminé la démonstration des autres comptes qui l'utilisent.

## 7. Sessions multiples

Pour montrer deux sessions du même compte sur la même machine, ouvrir temporairement un troisième terminal utilisant le même dossier de données, puis lancer une seconde connexion de bob. `who` doit afficher deux entrées distinctes. Fermer ensuite les sessions proprement.

Les tests automatisés reproduisent ce cas avec plusieurs pseudo-terminaux ; une répétition réelle permet aussi de vérifier le confort de la démonstration et les fenêtres affichées au jury.

<!-- pagebreak -->

## 8. Questions de révision

**Pourquoi un verrou ?** Deux terminaux peuvent modifier les mêmes fichiers. `flock` empêche l'entrelacement des opérations qui doivent observer et modifier un état cohérent, notamment les droits.

**Pourquoi ne pas garder le verrou pendant un mot de passe ?** Une saisie humaine peut durer longtemps. Le réseau doit rester utilisable. Le programme lit une référence, libère le verrou pour la saisie puis revérifie l'état avant la modification.

**Pourquoi un sel ?** Deux mots de passe identiques ne doivent pas produire le même enregistrement. Le sel aléatoire accompagne le hachage SHA-512 crypt et empêche la réutilisation directe d'un calcul précalculé unique pour tous les comptes.

**Pourquoi le PID seul est-il insuffisant ?** Le système peut réattribuer un numéro après la fin d'un processus. Le programme vérifie aussi l'identifiant de démarrage du système et le temps de démarrage du processus, issus de `/proc`.

**Que se passe-t-il après SIGKILL ?** Aucun gestionnaire du processus tué ne peut s'exécuter. Les opérations suivantes constatent l'absence du propriétaire et retirent la session. Pour INT, HUP et TERM, les gestionnaires nettoient directement les sessions.

**Pourquoi A et B restent-elles dans rusers pendant C ?** Les connexions sont imbriquées et conservées sur une pile. `exit` peut donc restaurer l'état précédent. Il s'agit du choix de comportement retenu et documenté.

**Que devient un wall -n pour un absent ?** Le programme conserve une copie par compte hors ligne. Sa première reconnexion transfère ce message vers sa session puis consomme la copie différée.

**Quelles limites annoncer ?** Simulation locale et non distribuée, compte Linux de confiance, absence de transaction générale après panne disque, absence d'accusé de réception persistant et prompt pouvant être visuellement coupé par une notification.

## 9. Contrôle avant remise

- Les deux membres savent retrouver chaque commande et son module.
- Les deux membres ont exécuté la démonstration sur leur propre environnement.
- Les rapports indiquent les résultats réellement obtenus et les limites connues.
- Les noms, la répartition réelle et les éventuelles consignes de format sont vérifiés par le binôme.
- Le créneau de présentation et les échéances sont confirmés avec l'enseignant.

Références : sujet LO14 2026-2027, code `9a56994`, README et `docs/VALIDATION.md`.
