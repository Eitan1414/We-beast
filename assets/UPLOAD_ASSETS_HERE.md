# Assets source — We Beast

Le dépôt est préparé pour recevoir les exports source du prototype avec des noms stables et sans espaces.

## Modèles 3D

```text
assets/source/player/dummy.glb

assets/source/maps/map_01.glb
assets/source/maps/map_02.glb

assets/source/props/ball.glb
assets/source/props/big_box.glb
assets/source/props/construction_cone.glb
assets/source/props/explosive_barrel.glb
assets/source/props/throwable_box_01.glb
assets/source/props/trashcan.glb

assets/source/decor/park_bench.glb
assets/source/decor/stairs_01.glb
assets/source/decor/low_wall_01.glb
assets/source/decor/railing_01.glb
assets/source/decor/ramp_01.glb
```

## Title screen

```text
assets/source/ui/title/title_background.png
assets/source/ui/title/title_logo.png
assets/source/ui/title/play.png
assets/source/ui/title/play_pressed.png
assets/source/ui/title/options.png
```

Les images fournies actuellement sont :

```text
title background.png  -> title_background.png   (1536x864)
Title.png             -> title_logo.png         (1536x512)
Play.png              -> play.png               (1536x358)
Play presed.png       -> play_pressed.png       (1536x358)
Option.png            -> options.png            (1536x358)
```

Il manque encore une version sélectionnée du bouton Options si l'on veut le même changement visuel que pour Play.

## Audio

```text
assets/source/audio/car_honk.mp3
```

Correspondance :

```text
Goofy ahh car honk sound effect.mp3 -> car_honk.mp3
```

Ce son est destiné à l'avertissement de la voiture de Map 2.

## Correspondance modèles Nomad

```text
Dummy 1.glb               -> dummy.glb
Map 1.glb                 -> map_01.glb
map 2.glb                 -> map_02.glb
Ball.glb                  -> ball.glb
big box.glb               -> big_box.glb
Cone de chantier.glb      -> construction_cone.glb
Explosive baril.glb       -> explosive_barrel.glb
throwable box 1.glb       -> throwable_box_01.glb
Trashcan.glb              -> trashcan.glb
Banc de parc.glb          -> park_bench.glb
escalier.glb              -> stairs_01.glb
Muret.glb                 -> low_wall_01.glb
Rambarde decor.glb        -> railing_01.glb
Rampe.glb                 -> ramp_01.glb
```

Les statistiques et rôles de chaque asset sont centralisés dans `assets/manifest.json`.

## Pipeline prévu

```text
GLB source
  -> validation
  -> conversion mesh/texture hors ligne
  -> format runtime Wii U
  -> buffers GX2
  -> rendu TV + GamePad
```

Pour l'UI et le son :

```text
PNG / MP3 source
  -> conversion/optimisation Wii U
  -> bundle runtime
  -> title screen / SFX
```
