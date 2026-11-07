# Rapport Codex — reprise LO14 2026–2027

## Mandat et périmètre

Demande directe de Selyan : conserver le même dépôt, archiver le travail sur le
mauvais sujet et préparer une nouvelle base Bash pour le sujet 2026–2027.
Aucun brief Opus créé. Il s'agit d'une base de départ, pas du projet fonctionnel.

## Relevé initial

- Dépôt : `https://github.com/JulesVRD/Shell_client-server_Project`.
- HEAD et branche récupérés : `2a93ae5616736f44fddcdc6c077420f8f71e81a0`, `main`.
- `git status --short --branch` : `## main...origin/main`, aucun changement local.
- `git log --oneline -20` : deux commits, `2a93ae5` et `e1e84b5`.
- Recon : lecture du README, de `src/client/vsh.c` et de `src/server/server.c`.
  Le README et le code confirment le sujet serveur d'archives en C/TCP.
- `relay-check.py` : aucun pair observé, mais `concurrence non vérifiée
  (ps indisponible)`. Ce constat n'est pas une preuve d'exclusivité.
- Clone initial dans `work/`, déplacé dans le dossier de livraison avant édition.

## Travail réalisé

- Branche locale `archive/vsh-2025-2026` créée au SHA initial : ancien projet intact.
- Branche locale `codex/rvsh-2026-2027` créée depuis le même historique.
- Retrait des fichiers propres au C/TCP et aux archives sur cette nouvelle branche.
  Ces fichiers restent conservés sur `main`, la branche d'archive et dans l'historique.
- Point d'entrée exécutable `rvsh` : aide, modes, validation des arguments.
- Remplacement du Makefile de compilation C par des raccourcis de vérification Bash.
- README réécrit, exigences du bon sujet recensées, questions ouvertes indiquées.
- PDF d'origine copié sans modification dans `docs/`.
- Documentation de reprise et de publication manuelle ; sauvegarde Git `.bundle`
  livrée à côté du dépôt après le commit local.

## Ce qui reste à développer

Toute la logique du réseau virtuel : initialisation, comptes, mots de passe,
droits, sessions, commandes connect/admin et messagerie. Les modes `-connect`
et `-admin` retournent volontairement 3 avec « non implémenté » ; aucun prompt
authentifié factice ni mot de passe prédéfini n'a été ajouté.

Le stockage, les interfaces entre modules, la concurrence et le mécanisme de
messagerie restent à décider ensemble. Voir `docs/CAHIER-DES-CHARGES.md`.

## Preuves de validation

Environnement exécuté : macOS arm64, GNU Bash 3.2.57. Linux n'a pas été testé.

Commande : `make test` (inclut `bash -n rvsh tests/cli.sh`). Sortie :

```text
bash -n rvsh tests/cli.sh
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
OK : connect annonce son état réel
OK : admin annonce son état réel
13 vérifications réussies.
```

Commande : `git diff --check`. Sortie vide, code 0.

Commande : `shasum -a 256 docs/LO14-Projet-2026-2027.pdf
/Users/selyan/Downloads/LO14-Projet-2026-2027.pdf`.
Les deux fichiers ont la même empreinte :

```text
cc5ef3f4d46bf506cd185aff3c08d4ada851f9b783476b7f1714635b38fdb047
```

Avant validation : `git rev-parse archive/vsh-2025-2026 main origin/main`
renvoie trois fois `2a93ae5616736f44fddcdc6c077420f8f71e81a0`.
Branche de travail : `codex/rvsh-2026-2027` ; modifications limitées à cette reprise.
Worktree de livraison :
`/Users/selyan/Documents/Codex/2026-11-07/j/outputs/Shell_client-server_Project`.

## Actions humaines restantes

1. Relire la nouvelle base à deux et convenir des choix listés dans le cahier des charges.
2. Publier la branche d'archive et la branche rvsh depuis cette copie locale
   (commandes dans `docs/REPRISE.md`), puis ouvrir et relire une PR vers `main`.
3. Développer et valider les fonctionnalités sous Linux avec plusieurs terminaux.
4. Préparer les rapports et confirmer le créneau de soutenance.

Aucun push, changement de dépôt distant, fusion vers main, déploiement ou envoi de mail.
