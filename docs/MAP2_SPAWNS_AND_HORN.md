# Map 2 — voiture, klaxon et donuts

## Voiture

Map 2 active le danger voiture uniquement quand `map_02.wbm` est chargé.

Cycle actuel :

1. attente aléatoire ;
2. phase d'avertissement ;
3. déclenchement du klaxon ;
4. traversée gauche→droite ou droite→gauche choisie aléatoirement ;
5. collision joueur = forte éjection horizontale + verticale, sans élimination instantanée ;
6. collision objet = forte impulsion qui peut envoyer une boîte ou un baril hors de la plateforme ;
7. nouvelle attente aléatoire.

Le klaxon runtime attendu est `car_honk.pcm`, dérivé du MP3 fourni en commençant vers 00:01.

## Donuts

Les quatre objets `Torus`, `Torus 1`, `Torus 2` et `Torus 3` de `map 2.glb` servent de points de spawn exclusifs.

Au début d'une manche Map 2, chacun reçoit aléatoirement exactement un des trois objets autorisés :

- petite boîte (`throwable_box_01`) ;
- grosse boîte (`big_box`) ;
- baril explosif (`explosive_barrel`).

Aucun de ces trois objets n'est généré ailleurs par le système de spawn Map 2.

## Physique des objets

Les quatre objets disposent maintenant d'une simulation volontairement légère pour la Wii U :

- gravité ;
- collision avec le sol ;
- frottement au sol ;
- chute hors de la map ;
- collision/poussée par les joueurs ;
- éjection par la voiture ;
- rayon de collision différent pour petite boîte, grosse boîte et baril.

Le but est de donner immédiatement du jeu aux objets sans introduire un solveur de rigid-body lourd.

Le jalon suivant reste la **saisie / porter / jeter**, puis le comportement propre du **baril explosif** (déclenchement, explosion, impulsion et destruction/respawn). Les quatre slots donuts resteront la source unique de respawn de ces objets sur Map 2.
