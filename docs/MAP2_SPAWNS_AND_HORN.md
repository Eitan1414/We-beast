# Map 2 — voiture, klaxon et donuts

## Voiture

Map 2 active le danger voiture uniquement quand `map_02.wbm` est chargé.

Cycle actuel :

1. attente aléatoire ;
2. phase d'avertissement ;
3. déclenchement du klaxon ;
4. traversée gauche→droite ou droite→gauche choisie aléatoirement ;
5. collision = forte éjection horizontale + verticale, sans élimination instantanée ;
6. nouvelle attente aléatoire.

Le klaxon runtime attendu est `car_honk.pcm`, dérivé du MP3 fourni en commençant vers 00:01.

## Donuts

Les quatre objets `Torus`, `Torus 1`, `Torus 2` et `Torus 3` de `map 2.glb` servent de points de spawn exclusifs.

Au début d'une manche Map 2, chacun reçoit aléatoirement exactement un des trois objets autorisés :

- petite boîte (`throwable_box_01`) ;
- grosse boîte (`big_box`) ;
- baril explosif (`explosive_barrel`).

Aucun de ces trois objets n'est généré ailleurs par le système de spawn Map 2.

Leur physique complète, la saisie/jeter et le respawn après destruction seront branchés sur ces mêmes slots dans le jalon suivant.
