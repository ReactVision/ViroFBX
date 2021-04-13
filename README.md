# ViroFBX

This repository maintains the converter from FBX formats to VRX formats.

FBX is an expansive and flexible 3D model format supported by most 3D authoring software. To load FBX files, use the ViroFBX script to convert the FBX file into a VRX file. The VRX file can then be loaded using [`<Viro3DObject>`](https://docs.viromedia.com/docs/3d-objects). The ViroFBX script for macOS can found in your project's `node_modules/react-viro/bin` directory.

This branch is for **Linux**. Support for macOS is in the [`master`](https://github.com/ViroCommunity/ViroFBX/tree/master) branch.

## Building
1. Make sure Java (version 8 works) is installed. `$JAVA_HOME` should be set to something like `/usr/lib/jvm/java-8-openjdk`.
2. Clone this repo and `cd` into it
3. `cd ViroFBX/`
4. `make`
This should make the `virofbx` binary.

## How to use
The following example shows how to convert a model into VRX format:

```console
$ ./virofbx path/to/model.fbx path/to/model.vrx
```

## Need help? Or want to contribute?
<a href="https://discord.gg/H3ksm5NhzT">
   <img src="https://discordapp.com/api/guilds/774471080713781259/widget.png?style=banner2" alt="ViroCommunity Discord server"/>
</a>
