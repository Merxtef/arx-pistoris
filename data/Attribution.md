# Test Data Attribution

This file records the sources used to create committed Pistoris test fixtures.
Generated native files, converted sidecars, and other derivatives are grouped
with their source assets.

## KayKit Dungeon Pack

- Author: [Kay Lousberg](https://www.kaylousberg.com/)
- Source:
  [KayKit Dungeon Pack](https://kaylousberg.itch.io/kaykit-dungeon-pack)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The level scenes are assembled derivatives. Their native files are generated
from the committed GLBs.

```text
data/fixtures/level/glb/level29/
data/fixtures/level/glb/level30/
data/fixtures/level/glb/level31/
data/fixtures/mount/game/graph/levels/level29..level31/
data/fixtures/mount/graph/levels/level29..level31/
data/fixtures/mount/graph/obj3d/textures/dungeon_texture*.png
```

## Modular RPG Characters

- Author: [System G6](https://opengameart.org/users/system-g6)
- Source:
  [Modular RPG Characters](https://opengameart.org/content/modular-rpg-characters)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The human model, head and armor variants, animation set, and Ambiance reference
previews are derivatives of this pack.

```text
data/fixtures/model/glb/head_*/
data/fixtures/model/glb/human_male/
data/fixtures/model/glb/*_armor/
data/fixtures/ambiance/glb/*/*.glb
data/fixtures/mount/game/graph/obj3d/interactive/npc/human_male/
data/fixtures/mount/graph/obj3d/anims/npc/human_male_*.tea
data/fixtures/mount/graph/obj3d/textures/tex_human_*.png
```

## Bloody Organ Texture

- Source: [Bloody Organ, Intestine or Flesh Texture](https://www.texturecan.com/details/137/)
- License: [CC0](https://www.texturecan.com/terms/)

The texture is used by the human model's gore material.

```text
data/fixtures/model/glb/human_male/human_male.glb
data/fixtures/mount/graph/obj3d/textures/gore.png
```

## Low Poly Fantasy Swords

- Author: [KevDev](https://opengameart.org/users/kevdev)
- Source:
  [Low Poly Fantasy Swords](https://opengameart.org/content/low-poly-fantasy-swords)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

```text
data/fixtures/model/glb/sword_00..sword_02/
data/fixtures/mount/game/graph/obj3d/interactive/items/weapons/sword_00..sword_02/
data/fixtures/mount/graph/obj3d/interactive/items/weapons/sword_00..sword_02/
data/fixtures/mount/graph/obj3d/textures/sword*.png
```

## W001 Composite Sword

- Author: [Price](https://opengameart.org/users/price)
- Source:
  [W001 Hand-Painted Composite Sword](https://opengameart.org/content/w001-hand-painted-composite-sword)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The source is used as the custom dagger OBJ fixture.

```text
data/fixtures/model/obj/custom_dagger/
data/fixtures/mount/game/graph/obj3d/interactive/items/weapons/custom_dagger/
data/fixtures/mount/graph/obj3d/interactive/items/weapons/custom_dagger/
data/fixtures/mount/graph/obj3d/textures/custom_dagger_texture.png
```

## Old Parchment Paper

- Author: [cron](https://opengameart.org/users/cron)
- Source: [Old Parchment Paper](https://opengameart.org/content/old-parchment-paper)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The image is used as the loading-screen background for the committed levels.

```text
data/fixtures/level/glb/level*/level*[loading].png
data/fixtures/mount/graph/levels/level*/loading.png
```

## Dark Shrine Loop

- Author: [qubodup](https://opengameart.org/users/qubodup)
- Source: [Dark Shrine Loop](https://opengameart.org/content/dark-shrine-loop)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The source is a remix of work by yd. The MP3 is referenced by the authored
Ambiance fixture and converted to WAV in the native fixture mount.

```text
data/fixtures/ambiance/glb/dark/dark_shrine/
data/fixtures/mount/sfx/ambiance/dark.amb
data/fixtures/mount/sfx/ambiance/qubodup-yd-darkshrineloop-opengameart.wav
```

## Medieval Exploration

- Author: [RandomMind](https://opengameart.org/users/randommind)
- Source: [Medieval Exploration](https://opengameart.org/content/medieval-exploration)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The authored Ambiance uses a shortened excerpt of the source MP3. Native
regeneration converts it to WAV.

```text
data/fixtures/ambiance/glb/explore/medieval_exploration/
data/fixtures/mount/sfx/ambiance/explore.amb
data/fixtures/mount/sfx/ambiance/exploration_0.wav
```

## 100 CC0 Metal and Wood SFX

- Author: [rubberduck](https://opengameart.org/users/rubberduck)
- Source:
  [100 CC0 Metal and Wood SFX](https://opengameart.org/content/100-cc0-metal-and-wood-sfx)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The OGG files are referenced by both Ambiance fixtures and by the human
gathering animation. Native regeneration converts them to WAV.

```text
data/fixtures/ambiance/glb/*/sfx/
data/fixtures/model/glb/human_male/sfx/
data/fixtures/mount/sfx/ambiance/*.wav
data/fixtures/mount/sfx/misc_04.wav
data/fixtures/mount/sfx/misc_08.wav
```

## Voiceover Pack (40+ lines)

- Author: [Kenney](https://opengameart.org/users/kenney)
- Source:
  [Voiceover Pack (40+ lines)](https://opengameart.org/content/voiceover-pack-40-lines)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The English number clips are used as-is. Native regeneration converts them
from Ogg Vorbis to WAV.

```text
data/fixtures/cinematic/numbers/speech/*[english].ogg
data/fixtures/mount/speech/english/*.wav
```

## German Vocabulary - Numbers

- Author: Human Robot
- Source:
  [German Vocabulary - Numbers.ogg](https://commons.wikimedia.org/wiki/File%3AGerman_Vocabulary_-_Numbers.ogg)
- License: [Public domain](https://creativecommons.org/publicdomain/mark/1.0/)

The German number clips are excerpts from the source recording. Native
regeneration converts them to WAV.

```text
data/fixtures/cinematic/numbers/speech/*[deutsch].ogg
data/fixtures/mount/speech/deutsch/*.wav
```

## 0-10 en Francais

- Author: [Sadiquecat](https://freesound.org/people/Sadiquecat/)
- Source: [0-10 en Francais](https://freesound.org/people/Sadiquecat/sounds/854808/)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The French number clips are excerpts from the source recording.

```text
data/fixtures/cinematic/numbers/speech/*[francais].wav
data/fixtures/mount/speech/francais/*.wav
```

## Flash Bang Sound

- Author: [teeeece](https://opengameart.org/users/teeeece)
- Source: [Flash Bang Sound](https://opengameart.org/content/flash-bang-sound)
- License: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

The WAV is used as-is by the authored Cinematic and its native fixture.

```text
data/fixtures/cinematic/numbers/sfx/flash_bang.wav
data/fixtures/mount/sfx/flash_bang.wav
```

## Pistoris Project Fixtures

- Author: Merxtef
- License: [Pistoris repository license](../LICENSE)

The level 9, JSON Model, and Numbers Cinematic source scenes, their original
visuals, and the black helper texture were created for Pistoris. The Cinematic
illustrations are embedded in its GLB. Native files are generated from the
source scenes.

```text
data/fixtures/json/
data/fixtures/cinematic/numbers/numbers.glb
data/fixtures/level/glb/level9/level9.glb
data/fixtures/model/glb/json_dummy/json_dummy.glb
data/fixtures/mount/graph/interface/illustrations/numbers.cin
data/fixtures/mount/graph/interface/illustrations/numbers_*.bmp
data/fixtures/mount/game/graph/levels/level9/fast.fts
data/fixtures/mount/game/graph/obj3d/interactive/fix_inter/json_dummy/json_dummy.ftl
data/fixtures/mount/graph/levels/level9/
data/fixtures/mount/graph/obj3d/anims/fix_inter/json_dice.tea
data/fixtures/mount/graph/obj3d/textures/json_model.png
data/fixtures/mount/graph/obj3d/textures/json_prison.png
data/fixtures/mount/graph/obj3d/textures/black.png
```
