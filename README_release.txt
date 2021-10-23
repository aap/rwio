DFF Import and Exporter 3.3 for 3ds max
=======================================

Compatibility
==============
 Works with 3ds max 2009, 2010, 2011, 2012, 2014, 2015, 2016, 2017, 2018, 2020, 2021, 2022 64 bit and 2009, 2010, 2011, 2012 32 bit

Installation
============
 - plugins/[yourarchitecture]/rwio[your_version].dli into [maxdir]/plugins
 - startup into [maxdir]/scripts

Features
========
 - new RW material implemented
    (to convert between Kam's material and this RW material
     use the MAXscript utility that comes with this plugin)
 - import PS2, Xbox (uncompressed only), mobile, d3d8 and d3d9 native geometry
 - import UV animations
 - SA extra vertex colors (RGB channel -1, alpha channel 9)
 - options accessible from MAXscript (see below)

Import options
==============
 - "Convert frame hierarchy" - Rotates the hierarchy and deletes a dummy node.
	Turning this off will import the raw hierarchy unchanged.
 - "Auto smooth" - Auto smooth imported meshes.
 - "Explicit normals" - Sets normals explicitly, you cannot change them with smoothing groups.
 - "Import as Standard Materials" - Create standard max materials instead of RW materials
 - "Take Material ID from Mesh" - use this to import KAM dffs, which have broken material IDs in Geometry

Export options
==============
 - "Lighting Flag" - Export the geometry with the lighting flag set.
 - "Vertex Normals" - Export normals with the geometry.
 - "Vertex Prelights" - Export vertex colors with the geometry.
 - "Tristrip" - Export mesh as tristrip (as opposed to trilist). Somewhat experimental.
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
To sort the order of children on import (which is important to have a predictable
hierarchy), set the user property "childNum".

Hierarchies that were originally exported from a Biped object by the official RW
exporter will have a "fakeBiped" user property set on the biped root that has to
do with orientation on export.

MAXscript
=========
 - "dffImp" is a struct that controls the following importer settings:
  - "convertHierarchy"
  - "autoSmooth"
  - "smoothingAngle"
  - "importStdMaterials"
  - "fixMaterialIDs"
  - "prepend" - not accessible through the import dialog. Prepends "!" to all names to avoid clashes when batch-importing. You're expected to rename all the imported nodes immediately after import

 - "dffExp" controls the following exporter settings:
  - "lighting"
  - "normals"
  - "prelight"
  - "tristrip"
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
 - export UV animations
 - lots more

Changelog
=========
1.0 - initial release
1.0a - fixed bug with PS2 geometry. Importing skinned geometry properly.
2.0 - improved hierarchy import; implemented exporter
2.0a - bug fixes
2.1 - handle scaled/translated geometry correctly; sort nodes by ID;
	cameras and lights correctly oriented; bug fixes
2.1a - bug fixes
3.0 - new version after long time, don't remember changes, mostly fixes
3.1 - explicit normals
3.2 - fixes
3.2a - fixed importing of weights
3.2b - fixed erronous export of vertex colors
3.2c - apparently fix didn't work? also max 2021
3.3 - add import std material, fix matid; export tristrip
3.4 - anm support; user data support; option to derive face winding from normals; fixes