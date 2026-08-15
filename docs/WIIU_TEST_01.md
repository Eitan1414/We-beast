# We Beast — Test Wii U 01

Premier test visuel natif WUT/GX2 de la V0.1.

## Ce que ce build teste

Le rendu est volontairement minimal et **ne charge pas encore les GLB**. Il sert à valider sur le vrai hardware Wii U :

- boucle principale WUT ;
- lecture du Wii U GamePad avec VPAD ;
- simulation `GameWorld` à pas fixe ;
- déplacement et saut ;
- chute hors arène = élimination ;
- Random Ball ;
- collision mortelle Ball/joueur ;
- rendu GX2 sur **TV + écran du GamePad**.

## Affichage de debug

```text
Grand carré vert/jaune : Map 1 simplifiée
Carré bleu             : joueur
Carré gris             : joueur éliminé
Losange rouge/orange   : Random Ball
```

La taille du joueur et de la Ball augmente légèrement quand leur hauteur `Y` augmente, afin de rendre les rebonds et le saut visibles dans cette vue du dessus.

## Contrôles

```text
Stick gauche  déplacer le joueur
A             sauter
-             recommencer la manche
```

## Fichiers à placer sur la SD

Layout principal attendu :

```text
sd:/wiiu/apps/webeast/webeast_wiiu.rpx
sd:/wiiu/apps/webeast/content/pos_col_shader.gsh
```

Le programme accepte aussi ce chemin de développement pour le shader :

```text
sd:/wut/content/pos_col_shader.gsh
```

Le shader `pos_col_shader.gsh` est déjà versionné dans :

```text
assets/runtime/shaders/pos_col_shader.gsh
```

## Compilation

Avec devkitPro + WUT installés :

```bash
mkdir -p build-wiiu
cd build-wiiu
/opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake ..
cmake --build . --parallel
```

Le build produit `webeast_wiiu.rpx`.

## Étape suivante après validation hardware

Remplacer progressivement les formes de debug par :

1. `Map 1.glb` ;
2. `Dummy 1.glb` ;
3. `Ball.glb` ;
4. les accessoires et décors ;
5. le vrai corps physique/ragdoll du joueur.
