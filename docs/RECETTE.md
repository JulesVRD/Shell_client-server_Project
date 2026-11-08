# Recette interactive du 8 novembre 2026

Code de référence : `9a56994`. Deux terminaux distincts ont été ouverts dans le
même conteneur Linux, avec `docker exec -it`, en complément des tests automatisés.
Le stockage se trouvait dans le `/tmp` éphémère du conteneur. Aucun compte réel
ni mot de passe personnel n'a été utilisé.

## Opérations exécutées

| Étape | Résultat observé |
|---|---|
| Premier `rvsh -admin` | Choix et confirmation du mot de passe puis prompt `root@hostroot >`. |
| Création de A, B, C et du compte alice | Les trois machines sont créées, puis les droits attribués. |
| `rvsh -connect machineA alice` dans le second terminal | Mot de passe demandé puis prompt `alice@machineA >`. |
| `who` et `finger` | Session courante et profil défini par `afinger` visibles. |
| `rconnect machineB`, puis `rconnect machineC` | Prompts B puis C ; `rusers` recense les trois connexions et root. |
| Deux `exit` | Retour sur B, puis sur A. |
| `host add interdit` depuis alice | Refus : commande réservée à l'administrateur. |
| `write alice@machineA ...` depuis root | Message reçu dans le second terminal sans nouvelle commande. |
| Sortie d'alice puis `wall -n ...` depuis root | Diffusion à root et conservation pour alice hors ligne. |
| Reconnexion d'alice | Message différé affiché avant le prompt. |
| Fermeture des deux shells | Le dossier des sessions est vide. |

Extraits de sortie, sans les saisies de mots de passe :

```text
alice@machineA > alice@machineB > alice@machineC >
rvsh : commande réservée à l’administrateur.

Messages pour alice@machineA :
root@hostroot : Message de recette en direct

Messages pour alice@machineA :
root@hostroot [wall -n] : Message différé de recette
alice@machineA >

Aucune session résiduelle
```

## Nettoyage et observation d'interface

Les sessions ont été quittées normalement, leur nettoyage vérifié, puis le
conteneur éphémère a été arrêté et supprimé. Aucun service de démonstration
n'est laissé en arrière-plan.

L'arrivée d'une notification peut couper visuellement le prompt ; le lecteur
Bash reste utilisable. C'est une limite d'interface connue, à annoncer et à
prendre en compte pendant la présentation.

Cette recette vérifie l'exécution des commandes. Elle ne remplace pas une
répétition personnelle de Jules et Selyan, notamment pour le rythme et
l'explication orale. Le guide dans `docs/rapports/guide-soutenance.md` fournit
le déroulé à reprendre ensemble.
