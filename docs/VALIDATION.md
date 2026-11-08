# Validation du réseau virtuel

Ce document décrit la validation de l'implémentation `rvsh` au commit `5024581`.
Les changements documentaires suivants ne modifient pas le code exécuté.

## Environnement

Les tests fonctionnels ont été exécutés sous Linux dans un conteneur basé sur
`python:3.12-alpine`, avec Bash 5.3.9, OpenSSL 3.5.9 et flock/util-linux 2.42.3.
Le programme est écrit en Bash ; Python intervient uniquement dans les tests.
La syntaxe a également été vérifiée avec Bash 3.2 sur macOS.

## Commandes exécutées

Depuis la racine du dépôt :

```bash
make lint
docker build -t rvsh-tests:local -f tests/Dockerfile .
docker run --rm --network none --read-only --tmpfs /tmp:rw,nosuid,nodev \
  -v "$PWD:/project:ro" -w /project rvsh-tests:local make test
```

L'image est construite localement. Pendant les tests, le code est monté en
lecture seule et le conteneur n'a pas de réseau. Chaque scénario utilise des
comptes synthétiques et un dossier temporaire supprimé à sa fin.

## Résultats

| Contrôle | Résultat |
|---|---|
| Syntaxe Bash du lanceur, des modules et du test CLI | Valide. |
| `shellcheck -x -a rvsh tests/cli.sh` | Code 0, aucun diagnostic. |
| Vérifications de l'interface de lancement | 11 réussites. |
| Scénarios d'intégration par pseudo-terminaux | 7 réussites en 6,033 secondes. |

Sortie de l'intégration :

```text
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

## Comportements vérifiés

- Initialisation, authentification, hachage des mots de passe et permissions des fichiers.
- Connexions A → B → C, puis retour vers B et A.
- Changement d'utilisateur sur la même machine et changement global du mot de passe.
- Plusieurs sessions d'un même utilisateur sur la même machine.
- Réception de `write` et `wall` sans action du destinataire.
- Réception unique d'un message `wall -n` à la prochaine connexion d'un compte hors ligne.
- Modifications concurrentes des droits depuis deux sessions administrateur.
- Refus d'accès sans droit, de suppression d'une ressource utilisée et d'injection de commandes.
- Nettoyage après SIGTERM et retrait d'une session morte après SIGKILL.
- Refus d'utiliser un dossier existant contenant des fichiers étrangers au programme.

## Documentation

Les trois schémas du README ont été analysés par `mermaid.parse`.
Les neuf blocs Bash ont été vérifiés avec `bash -n` et les seize liens locaux
et ancres du README ont été contrôlés. Le rendu visuel GitHub n'a pas été inspecté.

## Limites et suite

La simulation fonctionne sur un hôte partagé. Les processus sont identifiés par
leur PID ; une réutilisation de PID ou un redémarrage peut nécessiter de vérifier
les sessions résiduelles. Une notification peut couper visuellement le prompt.

La validation automatisée doit être complétée par une démonstration manuelle à
deux sur votre Linux/WSL. Les choix de comportement du README restent à relire
ensemble lors de la démonstration et de la préparation des rapports.


## Organisation des branches — 8 novembre 2026

Le projet courant est intégré dans `main` par avance rapide. L'ancien état de
`main`, au commit `2a93ae5`, est conservé dans `archive-projet-2025`.
La branche `feature/rvsh-2026-2027` est conservée. Cette opération et les mises
à jour des liens ne modifient pas le code testé.
