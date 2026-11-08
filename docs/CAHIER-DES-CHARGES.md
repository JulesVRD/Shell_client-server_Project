# Cahier des charges — LO14 2026–2027

Source : `LO14-Projet-2026-2027.pdf`, conservé dans ce dossier. Ce document
reformule les exigences ; le PDF reste la référence. Les commandes
ci-dessous sont implémentées. Leur validation automatisée est décrite dans le
README et le rapport ; la démonstration manuelle à deux reste à effectuer.

## Modes de lancement

- `rvsh -connect nom_machine nom_utilisateur` : vérifier l'existence de la machine
  et du compte, l'autorisation d'accès et le mot de passe ; afficher ensuite
  `nom_utilisateur@nom_machine >`.
- `rvsh -admin` : authentifier l'administrateur par mot de passe, puis afficher
  `root@hostroot >`. La machine virtuelle `hostroot` existe dès l'initialisation.
- L'administrateur dispose aussi des commandes du mode connect.

## Commandes obligatoires

| Contexte | Commande | Comportement attendu |
|---|---|---|
| Connect et admin | `who` | Sessions sur la machine courante, avec utilisateur, date et heure. Conserver plusieurs connexions d'un même utilisateur sur la même machine. |
| Connect et admin | `rusers` | Sessions sur tout le réseau, avec utilisateur, machine, date et heure. |
| Connect et admin | `rhost` | Liste des machines du réseau virtuel. |
| Connect et admin | `rconnect` | Connexion à une autre machine après vérification des droits. |
| Connect et admin | `su` | Changement d'utilisateur sans changer de machine. |
| Connect et admin | `passwd` | Changement du mot de passe de l'utilisateur pour tout le réseau. |
| Connect et admin | `finger` | Consultation des informations complémentaires d'un utilisateur. |
| Connect et admin | `write nom_utilisateur@nom_machine message` | Message à un utilisateur connecté sur la machine désignée. |
| Connect et admin | `exit` | Retour à la machine précédente si les connexions sont imbriquées ; sortie du shell à la dernière session. |
| Admin | `host` | Ajout ou retrait d'une machine. |
| Admin | `user` | Ajout ou retrait d'un utilisateur, attribution des accès aux machines et définition du mot de passe. |
| Admin | `wall message` | Message à tous les utilisateurs connectés du réseau. |
| Admin | `wall -n message` | Message à tous, y compris les utilisateurs hors ligne qui le reçoivent à leur prochaine connexion. |
| Admin | `afinger` | Renseignement des informations consultées avec `finger`. |

## Cas à vérifier pendant le développement

- Refus si machine ou compte inconnu, droit absent ou mot de passe incorrect.
- Refus du mode admin et de ses commandes sans authentification adaptée.
- Deux terminaux : même utilisateur, même machine, deux sessions distinctes.
- Parcours A → B → C, puis `exit` → B, puis `exit` → A.
- `su` conserve la machine ; `passwd` prend effet sur tout le réseau.
- `write` atteint la bonne destination et conserve un message contenant des espaces.
- `wall` atteint les utilisateurs connectés ; `wall -n` est aussi livré à la reconnexion.
- Fermeture normale et interruption d'un terminal : absence de session fantôme.
- Modifications depuis plusieurs terminaux : état cohérent et absence de perte de données.
- Les commandes inconnues du shell virtuel ne doivent pas devenir des commandes
  arbitraires du système hôte.

## Choix retenus pour cette version

Ces points précisent le sujet ; ils ne sont pas des exigences supplémentaires de l'énoncé.

- Stockage dans des fichiers locaux, choix validé directement par Selyan.
- Verrou exclusif partagé `flock` pendant les lectures/modifications de l'état.
- Machine `hostroot` et compte root créés au premier `rvsh -admin`, après choix
  et confirmation du mot de passe ; aucune valeur prédéfinie.
- Syntaxe détaillée des commandes documentée dans le README et `help`.
- Mots de passe SHA-512 crypt avec sel aléatoire, calculés par OpenSSL.
- Chaque connexion imbriquée conserve son enregistrement jusqu'à son `exit`.
- `write` vise toutes les sessions du compte sur la machine indiquée.
- `wall -n` diffuse à chaque session connectée et conserve un message par compte
  entièrement hors ligne, consommé à sa première reconnexion.
- Suppression de compte/machine et retrait d'un droit refusés s'ils sont utilisés.
- `su` remplace l'utilisateur de la connexion courante. Un utilisateur ordinaire
  ouvre le mode administrateur par `rvsh -admin`, pas par `su root`.
- Notifications vérifiées toutes les 0,2 seconde, en arrière-plan.
- Sessions liées au PID, au démarrage du système et à celui du processus pour détecter les PID réutilisés.

## Suite à effectuer par le binôme

- Revoir ensemble les règles ci-dessus et les confirmer avec l'enseignant si besoin.
- Faire une démonstration manuelle dans votre Linux/WSL, avec deux terminaux.
- Préparer les rapports et la présentation à partir du code réellement exécuté.

## Livrables et calendrier du sujet

- Travail en binôme.
- Premier rapport de quelques pages avant le 4 décembre 2026 : structure envisagée
  et choix particuliers. Préparation du texte seulement ; aucun envoi automatique.
- Rapport final de quelques pages avant le 10 janvier 2027.
- Présentation et test d'environ 20 minutes, annoncés pendant la première semaine
  de janvier 2027 ; créneau exact à confirmer.
