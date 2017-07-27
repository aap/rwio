RWio for 3ds max
================

This is an RW im-/exporter for 3ds max with support for GTA.

# How to build

Get premake5. Edit premake5.lua and have the projects point to your SDK version.
Build librw and librwgta (both on github) for the win-amd64-null or
win-x86-null platform and set the LIBRW and LIBRWGTA environment variables
to point to the respective directories.
Generate a VS project and build the version you need.
