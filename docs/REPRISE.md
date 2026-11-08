# Branches et historique du projet

## Organisation actuelle

| Branche | Contenu |
|---|---|
| `main` | Projet LO14 2026–2027 : réseau virtuel `rvsh` en Bash. |
| `archive-projet-2025` | Ancien serveur d'archives `vsh` en C, conservé sans modification. |
| `feature/rvsh-2026-2027` | Branche de développement du nouveau projet, conservée après son intégration dans `main`. |

Ces trois branches sont publiées sur GitHub. La branche d'archive pointe sur
`2a93ae5616736f44fddcdc6c077420f8f71e81a0`, état de l'ancien projet avant la reprise.
Les commits et leur attribution sont conservés. Le passage sur `main` est une
avance rapide, sans réécriture de l'historique.

## Récupérer le projet actuel

```bash
git clone --branch main https://github.com/JulesVRD/Shell_client-server_Project.git
cd Shell_client-server_Project
```

Depuis un clone existant, après sauvegarde des modifications locales :

```bash
git fetch origin
git switch main
git merge --ff-only origin/main
```

Si la dernière commande refuse une divergence, examiner les commits locaux
avec le binôme avant de poursuivre ; ne pas forcer ni effacer le travail local.

## Consulter l'ancien projet

Dans un dossier neuf :

```bash
git clone --branch archive-projet-2025 \
  https://github.com/JulesVRD/Shell_client-server_Project.git ancien-projet-lo14
```

Depuis un clone existant, sans changer de branche :

```bash
git fetch origin
git show origin/archive-projet-2025:README.md
```

## Travailler à deux

Partir de `main` à jour, puis créer une branche dédiée à la modification.
Relire les changements ensemble avant leur intégration. L'archive doit rester
une référence de l'ancien sujet.

## Copie transportable

La sauvegarde locale `rvsh-reprise.bundle` contient les branches et l'historique.
Pour la restaurer dans un dossier neuf :

```bash
git clone -b main /chemin/vers/rvsh-reprise.bundle rvsh-projet
cd rvsh-projet
git remote set-url origin https://github.com/JulesVRD/Shell_client-server_Project.git
git branch archive-projet-2025 origin/archive-projet-2025
```
