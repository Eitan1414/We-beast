# We Beast

Jeu de combat physique 3D original pensé pour **Wii U**, inspiré par les party-brawlers à personnages ragdoll.

## État actuel — V0.1

Le projet a maintenant deux couches :

1. un **cœur gameplay C++ portable**, testable sur PC ;
2. une première **cible native Wii U/WUT + GX2** (`webeast_wiiu.rpx`).

Déjà implémenté :

- simulation gameplay à pas fixe **60 Hz** ;
- déplacement analogique du joueur ;
- gravité et saut ;
- sortie de la plateforme / chute sous la kill-plane = élimination ;
- jusqu'à 4 états joueurs dans `GameWorld` ;
- obstacle spécial **Random Ball** ;
- rebonds et impulsions aléatoires de la Ball ;
- impact violent de la Ball = élimination ;
- détection de fin de manche / dernier joueur vivant ;
- contrôles GamePad : stick gauche = déplacement, `A` = saut, `-` = reset ;
- premier **rendu GX2 sur TV + GamePad** ;
- vue de debug de Map 1 avec joueur et Ball synchronisés au `GameWorld` ;
- convertisseur `tools/glb_to_wbm.py` pour transformer les GLB Nomad en meshes runtime légers ;
- tests C++ PC + workflow GitHub Actions.

## Assets V0.1

Les dernières versions optimisées sont référencées dans `assets/manifest.json` :

- `Dummy 1.glb` — joueur ;
- `Map 1.glb` — première arène ;
- `Ball.glb` — danger autonome ;
- grosse boîte / boîte lançable ;
- cône de chantier ;
- baril explosif ;
- banc ;
- escalier ;
- muret ;
- rambarde ;
- poubelle (ancienne version encore à optimiser).

Les GLB restent les **masters éditables**. Pour le runtime Wii U, le format `WBM1` conserve un mesh indexé, les couleurs de sommet et les UV sans embarquer toute la structure glTF.

Exemples avec les exports optimisés actuels :

```text
Map 1   ~6 Mo GLB  -> ~64 Ko WBM (géométrie)
Dummy   ~642 Ko    -> ~568 Ko WBM
Ball    ~236 Ko    -> ~203 Ko WBM
```

Les textures de Map 1 seront traitées séparément dans la passe texture GX2.

## Random Ball

La balle est un danger autonome de Map 1 :

- elle rebondit dans l'arène ;
- sa trajectoire reçoit régulièrement une impulsion aléatoire ;
- sa vitesse est plafonnée pour garder la simulation stable ;
- un contact léger n'élimine pas ;
- un impact suffisamment violent élimine immédiatement le joueur.

Les paramètres se trouvent dans `RandomBallConfig`.

## Rendu Wii U V0.1

Le premier rendu GX2 est volontairement simple afin de valider le hardware avant le chargement des modèles complets :

```text
Grand carré vert/jaune : arène de debug
Carré bleu             : joueur
Carré gris             : joueur éliminé
Losange rouge/orange   : Random Ball
```

La scène est rendue simultanément sur **la télévision et l'écran du Wii U GamePad**.

Voir `docs/WIIU_TEST_01.md` pour le layout SD et le test hardware.

## Contrôles Wii U actuels

```text
Stick gauche  déplacement
A             saut
-             recommencer le test
```

## Compiler les tests sur PC

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

La cible Wii U produit :

```text
webeast_wiiu.rpx
```

Le shader GX2 requis est versionné dans :

```text
assets/runtime/shaders/pos_col_shader.gsh
```

## Organisation

```text
assets/manifest.json          inventaire des modèles et budget géométrique
assets/runtime/               données converties pour le runtime Wii U
config/                       réglages des maps/gameplay
src/game/                     gameplay indépendant du rendu
src/math/                     mathématiques légères
src/platform/wiiu/            WUT, VPAD et GX2
tools/                        pipeline de conversion des assets
tests/                        simulations/tests PC
.github/workflows/            CI des tests gameplay
```

## Prochain jalon

**V0.2 visuelle** :

1. charger les fichiers `WBM1` depuis la SD ;
2. remplacer les formes de debug par `Map 1`, `Dummy` et `Ball` ;
3. ajouter la caméra 3D ;
4. convertir/appliquer les textures de Map 1 ;
5. ajouter les accessoires physiques ;
6. commencer le véritable corps physique/ragdoll et la saisie avec les mains.
