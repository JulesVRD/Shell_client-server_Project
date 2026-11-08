# Rapport Codex — réseau virtuel LO14 2026–2027

## Mandat et état initial

Selyan a demandé de poursuivre après la création de la base, puis a explicitement
validé le stockage du réseau virtuel dans des fichiers locaux.

Worktree : `/Users/selyan/Documents/Codex/2026-11-07/j/outputs/Shell_client-server_Project`.
Branche : `codex/rvsh-2026-2027`.
HEAD au début de cette tranche : `ef4cf3aa8f8d2fe51e0b0cd5060b3506a7d352ef`.
`git status --short --branch` : `## codex/rvsh-2026-2027`, arbre propre.
Recon : historique, fichiers du lanceur, tests et cahier des charges relus.
Aucun autre AGENTS.md ou brief présent dans ce clone.

La reprise initiale est conservée dans `ef4cf3a`. L'ancien projet reste sur
`archive/vsh-2025-2026`, `main` et `origin/main`, tous au SHA
`2a93ae5616736f44fddcdc6c077420f8f71e81a0`.

## Réalisation

- Initialisation interactive du mot de passe administrateur et de `hostroot`.
- Authentification, vérification des droits et refus des accès invalides.
- Stockage local privé, mot de passe SHA-512 crypt salé via OpenSSL et
  verrouillage des accès partagés par `flock`.
- Protection contre l'utilisation accidentelle d'un dossier de données contenant
  des fichiers sans rapport avec rvsh.
- Administration : `host`, `user`, droits grant/revoke et changement du mot de
  passe d'un utilisateur par l'administrateur.
- Sessions : `who`, `rusers`, `rhost`, `rconnect`, connexions imbriquées et `exit`.
- Identités et informations : `su`, `passwd`, `finger`, `afinger`.
- Messagerie : `write`, `wall`, `wall -n`, notification en arrière-plan et
  livraison différée à la première reconnexion d'un compte hors ligne.
- Plusieurs sessions distinctes du même compte sur la même machine.
- Nettoyage des sessions à la fermeture et suppression des sessions dont le
  processus est mort à la consultation ou lors d'un envoi.
- Tests CLI, tests d'intégration par pseudo-terminaux, environnement Docker Linux,
  README avec guide de démarrage et démonstration à deux terminaux.

Le code du programme reste en Bash. Python est utilisé uniquement dans les tests.
Aucun mot de passe réel n'a été créé : les tests utilisent des comptes synthétiques
et un stockage temporaire supprimé après chaque scénario.

## Choix de comportement

Le stockage local a été validé par Selyan. Les détails d'interface retenus sont
consignés dans le README et le cahier des charges : noms simples, connexions
imbriquées conservées jusqu'à leur `exit`, diffusion à toutes les sessions
correspondantes, retrait d'un droit et suppression refusés pendant son utilisation.
Ces règles restent à relire à deux et, si nécessaire, à confirmer avec l'enseignant.

## Preuves

### Analyse statique et syntaxe

Commande : `make lint`.

```text
shellcheck -x -a rvsh tests/cli.sh
```

Code 0, aucun diagnostic. Les modules sourcés sont aussi analysés grâce à `-a`.
Syntaxe vérifiée par `bash -n` sur le lanceur, les modules et le test CLI.
Commande : `git diff --check` : code 0, sortie vide.

### Exécution réelle sous Linux

L'image de test locale est construite depuis `tests/Dockerfile`, basée sur
`python:3.12-alpine`, avec Bash 5.3.9, OpenSSL 3.5.9 et flock/util-linux 2.42.3.
Aucun paquet n'a été installé globalement sur la machine.

Commandes exécutées :

```bash
docker build -t rvsh-tests:local -f tests/Dockerfile .
docker run --rm --network none --read-only --tmpfs /tmp:rw,nosuid,nodev \
  -v /Users/selyan/Documents/Codex/2026-11-07/j/outputs/Shell_client-server_Project:/project:ro \
  -w /project rvsh-tests:local make test
```

Le code est monté en lecture seule ; les données de test sont dans `/tmp` du
conteneur et le conteneur d'exécution n'a pas de réseau.

Sortie finale :

```text
bash tests/cli.sh
OK : aide longue
OK : aide courte
OK : mode obligatoire
OK : ancien mode refusé
OK : mode arbitraire refusé
OK : arguments superflus aide
OK : arguments superflus admin
OK : machine seule refusée
OK : machine vide refusée
OK : utilisateur vide refusé
OK : arguments superflus connect
11 vérifications réussies.
python3 tests/integration.py
test_connexions_imbriquees_su_et_mot_de_passe_global ... ok
test_dossier_existant_etranger_refuse_sans_modification ... ok
test_initialisation_authentification_et_permissions ... ok
test_initialisation_incomplete_et_mode_sans_reseau ... ok
test_modifications_concurrentes_et_session_tuee ... ok
test_refus_droits_noms_et_suppression_en_cours ... ok
test_sessions_multiples_messages_et_livraison_differee ... ok

Ran 7 tests in 6.033s

OK
```

Ces scénarios vérifient notamment : A → B → C et retours, `su` sur la même machine,
changement global et redéfinition admin du mot de passe, double connexion,
réception de `write` et `wall` sans action du destinataire, `wall -n` reçu une
seule fois après reconnexion, opérations concurrentes sur les droits, refus
d'accès et d'injection de commandes, SIGTERM et nettoyage après SIGKILL.

### Git et concurrence

Avant validation, HEAD reste `ef4cf3a`, sur la branche dédiée, avec uniquement les
changements de cette tranche. `main`, `origin/main` et la branche d'archive
restent au SHA initial `2a93ae5`.
`relay-check.py` ne signale aucun pair ; il précise toutefois
`concurrence non vérifiée (ps indisponible)`. Ce relevé n'est pas un verrou.
La livraison est commitée localement, puis la sauvegarde `rvsh-reprise.bundle`
est actualisée et vérifiée.

## Limites et actions restantes

- La cible d'exécution est Linux. La syntaxe Bash est aussi vérifiée sous Bash
  3.2 du poste macOS ; le programme n'est pas exécuté nativement sous macOS,
  où flock est absent. Le README décrit une démonstration via un conteneur partagé.
- Les notifications peuvent couper visuellement le prompt en cours de saisie.
- L'identification des processus repose sur leurs PID locaux. Un redémarrage ou
  une réutilisation de PID peut nécessiter de vérifier les sessions résiduelles.
  Le projet simule un réseau sur un hôte ; il ne vise pas l'isolation entre
  différents comptes du système d'exploitation.
- Aucun test ne prouve l'absence de tout défaut. Une démonstration manuelle à deux
  sur votre Linux/WSL et une relecture du code restent à effectuer.
- Préparer les rapports et la soutenance ; aucun envoi de document n'est effectué.
- Publier les deux branches et relire une PR lorsque Selyan le décide, selon
  `docs/REPRISE.md`.

Lors de la tranche de développement initiale : aucun push, fusion vers main, déploiement ou envoi de mail.


## Mise à jour documentaire — 8 novembre 2026

Mandat direct : refaire le README GitHub en s'inspirant de la présentation de la
stack Claude de Selyan, avec des schémas et une documentation claire.
État initial : branche `codex/rvsh-2026-2027`, HEAD `5024581`, arbre propre,
branche déjà publiée sur GitHub à la demande explicite de Selyan.

- README restructuré : table « Je veux… », prise en main à deux terminaux,
  responsabilités des modules, commandes, données, tests et limites.
- Trois schémas Mermaid : architecture, connexions imbriquées et messagerie.
- Exemples vérifiés contre le code, résultats des tests rattachés au commit validé.
- Guide de reprise corrigé : la branche de développement est publiée ; la branche
  d'archive reste locale. `main` contient toujours l'ancien sujet.
- Seule la structure de présentation de la stack a été utilisée ; aucun détail
  de son infrastructure personnelle n'a été repris dans le README.

Validation exécutée avec Mermaid et jsdom installés uniquement dans le dossier
temporaire de travail, sans dépendance ajoutée au projet ni installation globale :

```text
node work/readme-qa/check.mjs
3 schémas Mermaid valides ; 9 blocs Bash valides ; 16 liens locaux et ancres valides.
```

Les schémas ont été analysés avec `mermaid.parse`, les blocs Bash avec `bash -n`,
et les liens relatifs/ancres comparés aux fichiers et titres présents.
`git diff --check` : sortie vide, code 0.
Le rendu visuel dans GitHub n'a pas été inspecté. La suite fonctionnelle n'a pas
été relancée : ce lot modifie uniquement trois fichiers Markdown.

Publication prévue sur la branche existante `codex/rvsh-2026-2027`, selon le mandat
de publication déjà donné ; aucune fusion vers `main`.
