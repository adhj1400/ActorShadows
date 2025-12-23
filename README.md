# ActorShadows

An SKSE plugin to configure lights/transforms somehow attached to the player characters NiNode-tree.

## Configuration

### INI Settings (`Data/SKSE/Plugins/ActorShadows.ini`)

[Check it out!](https://github.com/adhj1400/ActorShadows/blob/main/ActorShadows.ini)

### Light Configuration

Configuration files are JSON files placed in `Data/SKSE/Plugins/ActorShadows/`. Each file configures one light source.

#### JSON Field Reference

All configuration files share the same structure. Create a separate JSON file for each light you want to configure.

- **type** (required): Specifies the light type. Must be one of:

  - `"HandheldLight"` - For torches, lanterns, and other held items as LIGHT records.
  - `"SpellLight"` - For spells which utilize MagicEffect-associated LIGHT records.
  - `"EnchantmentLight"` - For enchanted armor utilizing enchantments with associated LIGHT records.

- **formId** (required): The form ID of the light/spell/armor.

- **plugin** (required): The plugin file that contains the form (e.g., `"YourMod.esp"`).

- **rootNodeName** (required): The name of the root NIF node where the light is attached. Use NifScope to find the correct node name for your specific mesh.

- **lightNodeName** (required): The name of the actual light node to modify. Usually `"AttachLight"` for most lights.

- **offsetX**, **offsetY**, **offsetZ** (optional): Position offset in game units. Use these to fine-tune where the light appears relative to the attachment point. Useful to prevent light clipping with the actor.

- **rotateX**, **rotateY**, **rotateZ** (optional): Rotation in degrees around each axis. Use these to adjust the light's orientation. Useful for hiding the ugly "seam" that separates the north and south hemispheres of the shadow sphere.

#### Example Configurations

**Torch** (`Torch.json`):

```json
{
  "type": "HandheldLight",
  "formId": "0x01D4EC",
  "plugin": "Skyrim.esm",
  "rootNodeName": "TorchFire",
  "lightNodeName": "AttachLight",
  "offsetX": 2.0,
  "offsetY": 30.0,
  "offsetZ": -2.0,
  "rotateX": -20.0,
  "rotateY": 0.0,
  "rotateZ": 0.0
}
```

## Build Dependencies

- Visual Studio 2022
- [CMake](https://cmake.org/) 3.21+
- [vcpkg](https://github.com/microsoft/vcpkg)

## License

MIT License
