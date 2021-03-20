# ViroFBX

This repository maintains the converter from FBX formats to VRX formats.

FBX is an expansive and flexible 3D model format supported by most 3D authoring software. To load FBX files, use the ViroFBX script to convert the FBX file into a VRX file. The VRX file can then be loaded using <Viro3DObject>. The ViroFBX script can found in your project's node_modules/react-viro/bin directory.

Currently the ViroFBX tool runs only on **Mac OS X**. Support for other platforms is on the way.

## How to use

The following example shows how to convert a model into VRX format:

```
./ViroFBX path/to/model.fbx path/to/model.vrx
```
