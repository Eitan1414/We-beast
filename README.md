# We Beast

Jeu de combat physique 3D original pensé pour **Wii U**, inspiré par les party-brawlers à personnages ragdoll.

## État actuel — prototype Wii U

Le projet a maintenant :

1. un **cœur gameplay C++ portable** ;
2. un format runtime maison **WBM1** ;
3. une cible native **Wii U / WUT / GX2** (`webeast_wiiu.rpx`) ;
4. un **title screen navigable** ;
5. la première mécanique spécifique de **Map 2 : la voiture**.

Déjà implémenté :

- simulation gameplay à pas fixe **60 Hz** ;
- déplacement analogique, gravité et saut ;
- chute hors arène = élimination ;
- jusqu'à 4 états joueurs ;
- **Random Ball** avec rebonds/impulsions aléatoires et impact mortel ;
- détection de fin de manche ;
- rendu GX2 simultané **TV + GamePad** ;
- chargeur `WBM1` endian-safe pour le PowerPC big-endian de la Wii U ;
- chargement d'une map depuis la SD ;
- **Map 2 préférée au démarrage**, avec repli automatique sur Map 1 si `map_02.wbm` est absent ;
- title screen avec **PLAY / OPTIONS**, navigation à la croix et état sélectionné ;
- `A` ouvre PLAY/OPTIONS, `B` ferme l'overlay Options ;
- `+` retourne au title screen pendant une partie ;
- **voiture Map 2** : délai aléatoire, phase d'avertissement, passage gauche→droite ou droite→gauche et grosse impulsion au joueur touché ;
- la voiture n'élimine pas immédiatement : elle éjecte le joueur pour laisser la future mécanique d'accrochage au bord décider s'il survit ;
- tests C++ PC pour gameplay + parser WBM1 + impact voiture.

## Title screen

Les assets fournis sont enregistrés dans `assets/manifest.json` :

```text
title background.png
Title.png
Play.png
Play presed.png
Option.png
```

Le title screen Wii U est déjà **fonctionnel côté navigation**. Le rendu actuel reproduit provisoirement les couleurs et la disposition avec de la géométrie GX2. Le prochain passage UI remplacera ces formes par les PNG eux-mêmes via un pipeline texture GX2.

Contrôles du title screen :

```text
Croix haut/bas   choisir PLAY / OPTIONS
A                valider
B                fermer OPTIONS
```

## Map 2 — voiture

Quand `map_02.wbm` est chargé, la mécanique voiture est activée automatiquement :

```text
attente aléatoire
   ↓
avertissement / klaxon
   ↓
voiture traverse la map dans un sens aléatoire
   ↓
collision joueur = grosse éjection
   ↓
si le joueur tombe hors map = élimination
```

Le gameplay possède déjà un compteur `warningSerial` pour déclencher le SFX exactement une fois au début de chaque avertissement. Le fichier fourni `Goofy ahh car honk sound effect.mp3` est référencé comme `assets/source/audio/car_honk.mp3`. Le décodage/lecture audio Wii U sera branché après conversion vers un format runtime léger.

## Maps et assets runtime

Les GLB Nomad restent les **masters éditables**. Le jeu ne parse pas directement glTF sur Wii U : les modèles sont convertis hors ligne vers `WBM1`, beaucoup plus simple à charger.

Le convertisseur générique est :

```text
tools/glb_to_wbm.py
```

Pour Map 1, `tools/build_map_visual.py` peut créer une version intermédiaire où la texture du tapis est pré-échantillonnée dans les couleurs de sommets :

```bash
python -m pip install pillow
python tools/build_map_visual.py "Map 1.glb" map_01.wbm --grid 64
```

Pour Map 2 :

```bash
python tools/glb_to_wbm.py "map 2.glb" map_02.wbm
```

Fichiers SD :

```text
sd:/wiiu/apps/webeast/content/map_02.wbm
sd:/wiiu/apps/webeast/content/map_01.wbm        # fallback conseillé
sd:/wiiu/apps/webeast/content/pos_col_shader.gsh
```

Le programme accepte aussi le layout de développement `sd:/wut/content/`.

## Contrôles Wii U en jeu

```text
Stick gauche  déplacement
A             saut
-             recommencer la manche
+             retour title screen
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
assets/manifest.json          inventaire des modèles/UI/audio
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

1. connecter les vrais PNG du title screen au renderer GX2 ;
2. convertir et charger `map_02.wbm` dans le bundle Wii U ;
3. brancher le klaxon sur `warningSerial` ;
4. ajouter les quatre points de spawn « donuts » de Map 2 pour petite boîte / grosse boîte / baril explosif ;
5. remplacer les marqueurs debug par `Dummy`, `Ball` et les vrais objets physiques.
