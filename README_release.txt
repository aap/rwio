DFF Import and Exporter 2.1a for 3ds max
=======================================

Compatibility
==============
 Works with 3ds max [2009, 2010, 2011, 2012, 2014, 2015, 2016] x64

Installation
============
 - dffplg_[your_version].dli into [maxdir]/plugins
 - startup into [maxdir]/scripts

Features
========
 - new RW material implemented
    (to convert to Kam's material use the MAXscript utility that comes with this plugin)
 - import PS2, Xbox (uncompressed only), mobile, d3d8 and d3d9 native geometry
 - import UV animations
 - SA extra vertex colors (RGB channel -1, alpha channel 9)
 - options accessible from MAXscript (see below)

Import options
==============
 - "Convert frame hierarchy" - Rotates the hierarchy and deletes a dummy node.
	Turning this off will import the raw hierarchy unchanged.
 - "Auto smooth" - Auto smooth imported meshes.

Export options
==============
 - "Lighting Flag" - Export the geometry with the lighting flag set.
 - "Vertex Normals" - Export normals with the geometry.
 - "Vertex Prelights" - Export vertex colors with the geometry.
 - "Create HAnim Hierarchy" - Creates a HAnim Hierarchy (to be used with skinned models).
 - "Export Skinning" - Export skinned geometry attached to the hierarchy.
 - "World Space" - Export the model in world space.
 - "Node names" - Export frames with R* node names.
 - "Extra Vertex Colors" - Export R* extra (night) vertex colors with the geometry.
 - "File Version" - Set the RenderWare version of the file to be written.

How to export
=============
The exporter will export the complete hierarchy of the first selected object.
To export skinned geometry select the hierarchy, the skinned object will be
exported as well. If you select the skinned object it will be exported without
skin data or the hierarchy.

To assign IDs to nodes in the hierarchy set the user property "tag".
A negative ID causes the node and its children not to be part of the HAnim hierarchy.

Hierarchies that were originally exported from a Biped object by the official RW
exporter will have a "fakeBiped" user property set on the biped root that has to
do with orientation on export.

MAXscript
=========
 - "dffImp" is a struct that controls the following importer settings:
  - "convertHierarchy"
  - "autoSmooth"
  - "smoothingAngle"
  - "prepend" - not accessible through the import dialog. Prepends "!" to all names to avoid clashes when batch-importing. You're expected to rename all the imported nodes immediately after import

 - "dffExp" controls the following exporter settings:
  - "lighting"
  - "normals"
  - "prelight"
  - "worldSpace"
  - "createHAnim"
  - "skinning"
  - "extraColors"
  - "nodeNames"
  - "fileVersion"

TODO
====
 - SA 2dfx
 - SA collision
 - SA breakable mesh
 - import Kam's broken DFFs
 - export UV animations
 - export tri-strips

Changelog
=========
1.0 - initial release
1.0a - fixed bug with PS2 geometry. Importing skinned geometry properly.
2.0 - improved hierarchy import; implemented exporter
2.0a - bug fixes
2.1 - handle scaled/translated geometry correctly; sort nodes by ID;
	cameras and lights correctly oriented; bug fixes
2.1a - bug fixes