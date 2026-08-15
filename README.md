# We Beast

Jeu de combat physique 3D original pensé pour **Wii U**, inspiré par les party-brawlers à personnages ragdoll.

## État actuel — V0.1 visuelle

Le projet a maintenant :

1. un **cœur gameplay C++ portable** ;
2. un format runtime maison **WBM1** ;
3. une cible native **Wii U / WUT / GX2** (`webeast_wiiu.rpx`).

Déjà implémenté :

- simulation gameplay à pas fixe **60 Hz** ;
- déplacement analogique, gravité et saut ;
- chute hors de Map 1 = élimination ;
- jusqu'à 4 états joueurs ;
- **Random Ball** avec rebonds/impulsions aléatoires et impact mortel ;
- détection de fin de manche ;
- contrôles GamePad : stick gauche, `A`, `-` ;
- rendu GX2 simultané **TV + GamePad** ;
- chargeur `WBM1` endian-safe pour le PowerPC big-endian de la Wii U ;
- chargement de **Map 1 depuis la SD** ;
- rendu de la vraie géométrie/couleur runtime de Map 1 ;
- joueur et Ball encore affichés sous forme de marqueurs de debug ;
- tests C++ PC pour gameplay + parser WBM1.

## Map 1 et assets runtime

Les GLB Nomad restent les **masters éditables**. Le jeu ne parse pas directement glTF sur Wii U : les modèles sont convertis hors ligne vers `WBM1`, beaucoup plus simple à charger.

Le convertisseur générique est :

```text
tools/glb_to_wbm.py
```

Pour la première map, `tools/build_map_visual.py` peut aussi créer une version intermédiaire où la texture du tapis est pré-échantillonnée dans les couleurs de sommets. Cela rend la map reconnaissable avant l'arrivée du vrai pipeline de textures GX2.

```bash
python -m pip install pillow
python tools/build_map_visual.py "Map 1.glb" map_01.wbm --grid 64
```

Le fichier doit ensuite être placé sur la SD avec le shader :

```text
sd:/wiiu/apps/webeast/content/map_01.wbm
sd:/wiiu/apps/webeast/content/pos_col_shader.gsh
```

Le programme accepte aussi le layout de développement :

```text
sd:/wut/content/map_01.wbm
sd:/wut/content/pos_col_shader.gsh
```

## Contrôles Wii U actuels

```text
Stick gauche  déplacement
A             saut
-             recommencer la manche
```

## Random Ball

La Ball est un danger autonome : elle rebondit dans l'arène, reçoit régulièrement une impulsion aléatoire, garde une vitesse plafonnée et élimine le joueur lors d'un impact suffisamment violent.

## Compiler les tests PC

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Compiler pour Wii U

Avec devkitPro + WUT :

```bash
mkdir -p build-wiiu
cd build-wiiu
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ..
cmake --build . --parallel
```

La cible produit `webeast_wiiu.rpx`. Le shader GX2 est versionné dans :

```text
assets/runtime/shaders/pos_col_shader.gsh
```

## Organisation

```text
assets/manifest.json          inventaire des modèles
assets/runtime/               données runtime/shaders
config/                       réglages maps/gameplay
src/assets/                   lecteur WBM1 portable
src/game/                     gameplay indépendant du rendu
src/math/                     maths légères
src/platform/wiiu/            WUT, VPAD et GX2
tools/                        conversion GLB -> runtime
tests/                        simulations/tests PC
```

## Prochain jalon

**V0.2 3D** : remplacer le marqueur joueur par `Dummy`, remplacer le losange par la vraie `Ball`, ajouter une caméra 3D, puis brancher objets physiques, saisie et ragdoll actif.
