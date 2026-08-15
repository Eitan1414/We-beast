# État des assets source

Mesures effectuées sur les GLB reçus :

| Asset | Triangles source | Décision |
|---|---:|---|
| Dummy 1 | ~163 100 | réduire fortement avant runtime |
| Map 1 | ~713 984 | beaucoup trop lourde telle quelle ; créer collision simplifiée + version visuelle optimisée |
| Ball | ~196 608 | beaucoup trop lourde pour une simple sphère ; cible runtime très basse |
| big box | 224 | très bien |
| throwable box 1 | 224 | très bien |
| cône de chantier | ~11 408 | acceptable en source, à alléger |
| baril explosif | ~11 088 | acceptable en source, à alléger |
| poubelle | ~31 320 | à alléger |

## Principe Wii U

Les modèles Nomad sont conservés comme masters. Le build Wii U utilisera des copies optimisées. Les collisions ne suivront jamais tous les triangles du modèle visuel : balle=sphère, boîtes=AABB/OBB, baril/poubelle=capsule ou cylindre simplifié, map=maillage de collision séparé.

Cela permettra de garder beaucoup d'objets physiques à l'écran sans faire exploser le coût CPU de la physique ni la mémoire GPU.
