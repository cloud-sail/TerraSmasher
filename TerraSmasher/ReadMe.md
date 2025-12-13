# TerraSmasher

## Game Material Workflow
> [Free PBR](https://freepbr.com/)
- Download the "bl" version (My Engine is using OpenGL UV Coords, RHI is DirectX12)
- Use ImageMagick to merge AO, Roughness, Metalic, Height into ORMH png (`TextureTools/PBR_O_R_M_H.bat`)
- PNG: `xxx_albedo.png`, `xxx_normal-ogl.png`, `xxx_ormh.png`, `xxx_emissive.png`
- DDS: Using Nvidia Texture Tools Exporter to convert png to dds (has mipmap)
	- `TextureTools/*.dpf` is the preset

## Density Point Cloud Workflow (.dcloud file)
- `HoudiniProjects/Voxelizer.hipnc`


