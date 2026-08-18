# We Beast V0.1 — solo combat test

This milestone is intentionally tiny: validate the first physical combat loop on real Wii U hardware before adding more characters, abilities or maps.

## Scene

- Map 1 is loaded first when `map_01.wbm` is present on the SD card.
- Player 1 is controllable.
- Player 2 is a no-AI training Dummy. It never walks or attacks by itself.
- Random Ball, Map 2 car and Map 2 props are disabled.
- The training Dummy respawns automatically after falling out of the arena so the test can be repeated alone.

## Controls

```text
Left stick   Move
A            Jump
Y            Punch
ZR (hold)    Grab a nearby Dummy
ZR (release) Throw the grabbed Dummy
-            Reset the training scene
+            Return to title screen
```

## Combat behaviour

### Punch

A punch checks for the closest target in front of the player and applies physical horizontal + vertical knockback. V0.1 does not add a long stun.

### Grab / throw

Holding ZR near the target attaches it in front of the player. The target remains physically carried while the player moves. Releasing ZR throws it in the current facing direction with forward and upward velocity.

The intended test is to carry the training Dummy toward a Map 1 edge and throw it into the void.

## Debug colours

Until the animated Prisma3D Dummy renderer is connected:

```text
Blue    controlled player
Red     training Dummy
Yellow  training Dummy currently grabbed
Grey    eliminated/falling state
```

The Random Ball marker is hidden in this milestone.

## Scope after this test

Only after punch/grab/throw feel correct on Wii U should the project connect the animated Dummy mesh and the Punch V5 animation, then expand toward multiplayer and abilities.
