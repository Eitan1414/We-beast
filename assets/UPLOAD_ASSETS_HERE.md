# V0.1 — fichiers GLB à ajouter

Le code et les chemins sont déjà prêts. Les pièces jointes binaires du chat ne peuvent pas être envoyées directement au dépôt par le connecteur GitHub actuel, donc place les exports Nomad optimisés avec **exactement** ces noms :

```text
assets/source/player/dummy.glb
assets/source/maps/map_01.glb

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
```

## Correspondance avec les fichiers Nomad fournis

```text
Dummy 1.glb               -> dummy.glb
Map 1.glb                 -> map_01.glb
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
```

Les statistiques et rôles de chaque asset sont dans `assets/manifest.json`.

Une fois ces fichiers présents, le prochain pipeline est :

```text
GLB source
  -> validation
  -> conversion mesh/texture hors ligne
  -> format runtime Wii U
  -> buffers GX2
  -> rendu TV + GamePad
```
