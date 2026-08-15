# We Beast

Jeu de combat physique 3D original pensé pour **Wii U**, inspiré par les party-brawlers à personnages ragdoll.

## État actuel — V0.1

Le projet a maintenant deux couches :

1. un **cœur gameplay C++ portable**, testable sur PC ;
2. une première **cible native Wii U/WUT** (`webeast_wiiu.rpx`).

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
- premier harness Wii U : stick gauche = déplacement, `A` = saut, `-` = reset ;
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

> Les GLB sont les assets source. Le connecteur utilisé pour développer le dépôt ne permet pas encore d'envoyer directement ces binaires : ils doivent encore être placés dans les chemins `target_path` indiqués dans `assets/manifest.json` avant le rendu 3D réel.

## Random Ball

La balle est un danger autonome de Map 1 :

- elle rebondit dans l'arène ;
- sa trajectoire reçoit régulièrement une impulsion aléatoire ;
- sa vitesse est plafonnée pour garder la simulation stable ;
- un contact léger n'élimine pas ;
- un impact suffisamment violent élimine immédiatement le joueur.

Les paramètres se trouvent dans `RandomBallConfig`.

## Contrôles Wii U actuels

```text
Stick gauche  déplacement
A             saut
-             recommencer le test
```

Il s'agit encore d'un **harness gameplay** : les coordonnées du joueur et de la Ball sont affichées dans la console WUT. Le rendu GX2 de Map 1 et des GLB est la prochaine étape.

## Compiler les tests sur PC

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Compiler pour Wii U

WUT recommande l'environnement `wiiu-dev` de devkitPro. Une fois installé :

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

## Organisation

```text
assets/manifest.json          inventaire des modèles et budget géométrique
config/                       réglages des maps/gameplay
src/game/                     gameplay indépendant du rendu
src/math/                     mathématiques légères
src/platform/wiiu/            couche native WUT/GamePad
tests/                        simulations/tests PC
.github/workflows/            CI des tests gameplay
```

## Prochain jalon

**V0.1 visuelle Wii U** :

1. intégrer les GLB optimisés dans le dépôt ;
2. convertir les meshes/textures vers un format runtime adapté ;
3. initialiser le rendu GX2 ;
4. afficher Map 1, Dummy et Ball ;
5. synchroniser leur transform avec `GameWorld` ;
6. ajouter les premiers objets physiques saisissables.
