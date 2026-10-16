# Life Again OAR Bridge

This optional companion SKSE plugin exposes Life Again follower injury state to Open Animation Replacer (OAR). It registers these OAR conditions:

- `LifeAgainIsInjured`: true for light or heavy Life Again injuries.
- `LifeAgainIsLightlyInjured`: true while the follower has only light injuries.
- `LifeAgainIsHeavilyInjured`: true only for heavy Life Again injuries.
- `LifeAgainHasArmInjury`: true for deep-cut and puncture injuries, selecting the wounded-arm animation set.
- `LifeAgainHasLegInjury`: true for crush injuries, selecting the limping leg animation set.

Use these conditions in an OAR animation configuration to select animation sets for injured followers. The bridge contains no animation assets; a ready-to-install local animation package is in `distribution/Life Again Injury Animations`. OAR and Life Again must both be installed; without OAR, the bridge logs that its conditions are unavailable and does not affect the main plugin.

When RaceMenu is installed, the bridge applies one color-tinted wound overlay per distinct wound type, using available body overlay slots. The texture is selected from the assets provided by **Wounds Overlays For RaceMenu** (mod 38547); install and enable that texture mod in MO2. Free overlay slots are used so existing overlays are left intact. Life Again's treatment, death cleanup, and revival clear the bridge's overlays. RaceMenu and the wound texture mod are optional; without them, the injury system and OAR conditions continue to work.

Build with `-DBUILD_LIFEAGAIN_OAR_BRIDGE=ON`. Install the generated `LifeAgainOARBridge.dll` beside `LifeAgain.dll` in `Data/SKSE/Plugins`.

The OAR API files under `include/OAR` are copied from [OpenAnimationReplacer-ExamplePlugin](https://github.com/ersh1/OpenAnimationReplacer-ExamplePlugin) for API interoperability. The SKEE interface declaration is adapted from [SKSE64Plugins/skee64](https://github.com/expired6978/SKSE64Plugins/tree/master/skee64). No wound textures are copied into this project. The optional animation package contains local copies from the user-installed Wounds animations 1.1.1 mod; do not redistribute them without the original author's permission. This bridge and its OAR API files are licensed under GPL-3.0; see [COPYING](COPYING). This separate target does not change the license of the main LifeAgain plugin.
