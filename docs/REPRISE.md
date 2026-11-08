# Reprise du dépôt

## État conservé

Dépôt d'origine : https://github.com/JulesVRD/Shell_client-server_Project

Base vérifiée : `2a93ae5616736f44fddcdc6c077420f8f71e81a0`.
Les deux commits historiques et leur attribution sont conservés.

- `main` et `origin/main` : état récupéré du dépôt, inchangé localement.
- `archive/vsh-2025-2026` : sauvegarde locale exacte de cet état, sans modification.
- `feature/rvsh-2026-2027` : nouvelle base Bash, issue de cet historique.

Les anciens fichiers sont retirés uniquement de la nouvelle branche. Ils restent
consultables sur la branche d'archive et dans l'historique Git. Éviter toute
réécriture d'historique ou envoi forcé.

## Vérifier ou consulter

Depuis cette copie locale :

```bash
git status --short --branch
git branch -v
git log --oneline --all --decorate
git ls-tree -r --name-only archive/vsh-2025-2026
# Lire l'ancien README sans changer de branche :
git show archive/vsh-2025-2026:README.md
```

## Publication et récupération

La branche `feature/rvsh-2026-2027` est publiée sur GitHub. `main` reste inchangée.
La branche `archive/vsh-2025-2026` est conservée localement ; le même ancien
code reste consultable sur `main` et dans l'historique distant.

Pour récupérer la version actuelle dans un dossier neuf :

```bash
git clone --branch feature/rvsh-2026-2027 https://github.com/JulesVRD/Shell_client-server_Project.git
```

Depuis la copie de livraison, Selyan peut également publier l'archive locale
et les mises à jour de la branche de développement :

```bash
git push origin archive/vsh-2025-2026
git push -u origin feature/rvsh-2026-2027
```

Créer ensuite une pull request de `feature/rvsh-2026-2027` vers `main`, à relire à
deux avant fusion. Ne pas exécuter ces commandes dans un autre clone sans y
avoir d'abord transféré les branches.

## Copie transportable

Le fichier `rvsh-reprise.bundle` livré à côté de ce dépôt contient les branches
locales et l'historique. Pour travailler ailleurs, utiliser un dossier neuf :

```bash
git clone -b feature/rvsh-2026-2027 /chemin/vers/rvsh-reprise.bundle rvsh-projet
cd rvsh-projet
git remote set-url origin https://github.com/JulesVRD/Shell_client-server_Project.git
# Le clone d'un bundle place aussi les autres branches dans origin/*.
git branch archive/vsh-2025-2026 origin/archive/vsh-2025-2026
```

Le changement éventuel de nom du dépôt GitHub peut attendre ; il n'est pas
nécessaire au démarrage du nouveau sujet.
