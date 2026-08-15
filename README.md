# We Beast

Jeu de combat physique 3D original pensé pour **Wii U**, inspiré par les party-brawlers à personnages ragdoll.

## État actuel

Prototype V0.1 en cours :

- personnage source `dummy.glb` ;
- première arène `map_01.glb` ;
- accessoires physiques : boîtes, cône, baril explosif, poubelle ;
- obstacle spécial **Random Ball** ;
- première simulation C++ du comportement de la balle.

## Random Ball

La balle est un danger autonome de la map :

- elle rebondit dans l'arène ;
- sa trajectoire reçoit régulièrement une impulsion aléatoire pour rester imprévisible ;
- sa vitesse est limitée afin de garder une physique stable ;
- un simple contact ne tue pas forcément ;
- un impact suffisamment violent élimine immédiatement le joueur touché.

Les valeurs sont regroupées dans `RandomBallConfig` pour pouvoir régler facilement la difficulté.

## Organisation

```text
assets/source/player/       modèles joueur source
assets/source/maps/         maps source
assets/source/props/        objets interactifs source
src/game/                   logique de gameplay indépendante du rendu
src/math/                   petites primitives mathématiques
tests/                      tests/simulations PC
```

## Important pour la Wii U

Les fichiers GLB actuels sont conservés comme **assets source**. Certains sont encore beaucoup trop détaillés pour être utilisés tels quels dans le build final Wii U. Une passe d'optimisation/LOD et une conversion vers un format runtime plus léger seront faites avant l'intégration GX2.

Le cœur gameplay est volontairement écrit en C++ léger et sans dépendance lourde afin de pouvoir être branché ensuite sur le runtime natif Wii U/WUT.
