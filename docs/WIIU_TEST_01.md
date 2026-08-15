# We Beast — Test Wii U 01

Premier test visuel natif WUT/GX2 de la V0.1.

## Ce que ce build teste

- boucle principale WUT ;
- lecture du Wii U GamePad avec VPAD ;
- simulation `GameWorld` à pas fixe ;
- déplacement, saut et chute hors arène ;
- Random Ball et impact mortel ;
- parser `WBM1` sur PowerPC big-endian ;
- chargement de **Map 1 depuis la SD** ;
- rendu GX2 sur **TV + GamePad**.

## Affichage

Map 1 n'est plus remplacée par un simple grand carré : le renderer charge `map_01.wbm` et dessine les faces orientées vers le haut. Pour le bundle de test actuel, la texture du tapis est pré-échantillonnée en couleurs de sommets afin de rendre les routes/bâtiments visibles sans pipeline texture GX2 complet.

Le joueur reste un carré bleu et la Random Ball un losange rouge/orange. Ils seront remplacés par leurs vrais meshes WBM dans le jalon suivant.

## Contrôles

```text
Stick gauche  déplacer le joueur
A             sauter
-             recommencer la manche
```

## Layout SD principal

```text
sd:/wiiu/apps/webeast/webeast_wiiu.rpx
sd:/wiiu/apps/webeast/content/map_01.wbm
sd:/wiiu/apps/webeast/content/pos_col_shader.gsh
```

Layout de développement également accepté :

```text
sd:/wut/content/map_01.wbm
sd:/wut/content/pos_col_shader.gsh
```

Si `map_01.wbm` est absent ou invalide, le build quitte avec le code `-3`. Si le shader ne peut pas être chargé, il quitte avec `-4`.

## Générer Map 1 runtime

```bash
python -m pip install pillow
python tools/build_map_visual.py "Map 1.glb" map_01.wbm --grid 64
```

Le mode `--grid 64` produit environ 8 192 triangles, ce qui reste léger pour ce test GX2 tout en conservant suffisamment de couleurs pour reconnaître le tapis.

## Compilation Wii U

```bash
mkdir -p build-wiiu
cd build-wiiu
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ..
cmake --build . --parallel
```

## Étape suivante

1. charger `dummy.wbm` et l'utiliser à la place du carré joueur ;
2. charger `ball.wbm` et l'utiliser à la place du losange ;
3. passer de la vue du dessus à une caméra 3D ;
4. ajouter le vrai sampling texture GX2 ;
5. brancher les accessoires physiques puis le ragdoll actif.
