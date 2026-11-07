# Cahier des charges — LO14 2026–2027

Source : `LO14-Projet-2026-2027.pdf`, conservé dans ce dossier. Ce document
reformule les exigences ; le PDF reste la référence. Toutes les fonctions
ci-dessous sont **à développer**, sauf l'analyse des arguments de lancement.

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

## Ordre de travail proposé

1. Valider le modèle des machines, comptes, droits et sessions ; choisir le
   stockage et les règles de modification concurrente.
2. Développer l'initialisation, l'authentification et le mode admin (`host`, `user`).
3. Développer les sessions connect, puis `who`, `rusers`, `rhost`, `rconnect`, `exit`.
4. Ajouter `su`, `passwd`, `finger`, `afinger`.
5. Développer `write`, `wall` et la livraison différée de `wall -n`.
6. Valider à plusieurs terminaux sous Linux et préparer les rapports/démonstrations.

Répartition possible, à valider entre vous : un membre sur comptes, machines,
droits et administration ; l'autre sur sessions, navigation et messagerie.
Les interfaces communes doivent être convenues avant cette séparation.

## Questions ouvertes avant implémentation

Le sujet ne fixe pas tous les détails. Ne pas les présenter comme des exigences :

- Syntaxe détaillée de `host`, `user`, `afinger` et paramètres de `rconnect`.
- Initialisation du mot de passe administrateur ; stockage et vérification des
  mots de passe, sans mot de passe de démonstration versionné.
- Format de stockage, identification des sessions et gestion des accès concurrents.
- Sessions A/B suspendues lors d'une connexion à C : visibilité dans `who` et `rusers`.
- Destinataires de `write` lorsqu'un compte a plusieurs sessions sur la même machine.
- Comportement si une machine ou un compte possédant des sessions actives est supprimé.
- Délai de réception des messages et règles de livraison après reconnexion.

## Livrables et calendrier du sujet

- Travail en binôme.
- Premier rapport de quelques pages avant le 4 décembre 2026 : structure envisagée
  et choix particuliers. Préparation du texte seulement ; aucun envoi automatique.
- Rapport final de quelques pages avant le 10 janvier 2027.
- Présentation et test d'environ 20 minutes, annoncés pendant la première semaine
  de janvier 2027 ; créneau exact à confirmer.
