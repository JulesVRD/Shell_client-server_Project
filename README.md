# LO14 — Réseau virtuel de machines (`rvsh`), 2026–2027

Projet de Jules et Selyan. Le sujet de référence est conservé dans
[docs/LO14-Projet-2026-2027.pdf](docs/LO14-Projet-2026-2027.pdf).

## Reprise du bon sujet

Le projet précédent était un serveur d'archives `vsh` en C, correspondant au
sujet 2025–2026. Il est conservé intégralement dans la branche
`archive/vsh-2025-2026`, au commit `2a93ae5616736f44fddcdc6c077420f8f71e81a0`.
L'historique et les contributions existantes sont conservés.

La branche `codex/rvsh-2026-2027` prépare le sujet actuel : une commande **Bash**
`rvsh`, avec deux modes :

```bash
./rvsh -connect nom_machine nom_utilisateur
./rvsh -admin
```

Les machines et utilisateurs sont virtuels. Le sujet ne demande pas de serveur
TCP ni de format d'archive. Les anciens fichiers C, archives et profils de
débogage C ne font donc pas partie de cette base.

## État réel de cette base

Disponible : aide, vérification des arguments et tests de ce point d'entrée.
**Le réseau virtuel, l'authentification et toutes les commandes interactives
restent à développer.** Un appel valide à `-connect` ou `-admin` affiche
explicitement « non implémenté » et retourne le code 3, sans ouvrir de session.
Aucun compte, mot de passe ou état persistant n'est créé.

## Prérequis et vérification

Cible du projet : Linux avec Bash. La base utilise une syntaxe compatible Bash
3.2 et versions suivantes. Aucun compilateur C ni installation globale requis.
`make` est facultatif ; il sert uniquement de raccourci pour les vérifications.

```bash
./rvsh --help
make test
# Sans make :
bash -n rvsh tests/cli.sh
bash tests/cli.sh
```

Codes de retour : `0` pour l'aide, `2` pour une invocation invalide, `3` pour un
mode valide dont le fonctionnement reste à développer.

## Fichiers

- `rvsh` : point d'entrée Bash et aiguillage des deux modes.
- `tests/cli.sh` : vérifications de l'interface en ligne de commande.
- `docs/CAHIER-DES-CHARGES.md` : exigences, cas de validation et points à clarifier.
- `docs/REPRISE.md` : branches, conservation de l'ancien travail et publication manuelle.
- `RAPPORT-CODEX.md` : périmètre réalisé, preuves et suite à donner.

## Suite du développement

Définir ensemble le stockage, l'identification des sessions et la livraison des
messages avant de coder ces parties. Le cahier des charges propose un ordre de
travail, sans fixer ces choix. L'ensemble des commandes demandées est obligatoire.

Échéances indiquées dans le sujet : premier rapport avant le **4 décembre 2026**,
rapport final avant le **10 janvier 2027** ; présentation et test annoncés pendant
la première semaine de janvier 2027. Confirmer le créneau de soutenance avec l'enseignant.
