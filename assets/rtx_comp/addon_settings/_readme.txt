Files in 'addon_settings' are automatically loaded on game start and are applied in inverse alphabetical order (lower to higher priority).
Community modders should use the addon_settings system to modify comp-mod / remix settings instead of directly editing the comp_settings.toml / rtx.conf files.

Files in 'addon_settings/presets' are listed in a listbox inside the [Setting Presets] in the Comp Settings tab of the F4 menu
and can be manually applied in-game by the user to quickly change the appearance of the game.

.conf files:
    - contain remix variables:

    ---- Format:
    rtx.dust.maxParticleSize = 6.0



.toml files:
    - contain compatibility mod settings with a version comment (required)
    - if the default value of a compatibility setting changes dramatically due to adjusted logic,
      outdated and incompatible addon settings will be skipped and that specific setting needs to be adjusted by the modder


    ---- Format:
    # Ver: 1.3.0
    remix_override_rtxdi_samplecount = 60