Heightmaps
==========

Heightmaps are tracked using the set of classes from heightmap.h.

.. contents::
   :local:

FixedHeightmap
--------------

FixedHeightmap contains a square chunk of heightmap data. This class is used
to hold both, the master heightmap data and the sector heightmap portions.
These different types of heightmap data chunks can have different sizes.

Internally, the FixedHeightmap code is trying to be able to accomodate any
rectangular chunk of heightmap data but this functionality is not exposed
outside.

UnlimitedHeightmap
------------------

UnlimitedHeightmap contains a heightmap without limits. It stores it in a
collection of FixedHeightMap-s that contain the actual heightmap data.

Master Heightmap
----------------

The "master heightmap" is contained in the class `UnlimitedHeightmap
<heightmap.rst#unlimitedheightmap>`_.

The size of the master height map chunk is
configurable using the "heightmap_blocksize" configuration variable. Changes
to this variable don't affect existing maps as the UnlimitedHeightmap map
instance stores this value into its serialization stream.
The default block size is 128.

Value Generators
----------------

The heightmap is generated using three factors: "random maximum", "random
factor" and "base". These values are not always constants but can be
functions of two variables that specify the position on the heightmap. These
functions are represented by value generator classes.

The "base" determines the base height of the terrain at a given point.

The "random maximum" determines the overall size of the terrain features.

The "random factor" determines how "rough" the terrain is. Small values
generate flat terrain, large values generate "spiky" mountains which later
deteriorate into "needles".

Increasing "random maximum" shall be accompanied with decreasing "random
factor" to get roughly the same terrain but with bigger features. Compare
for example "random maximum"=60 and "random factor"=0.6 versus "random
maximum"=700 and "random factor"=0.2.

ConstantGenerator returns a predefined value for each X,Y

LinearGenerator represents a linearly sloped plane function. Its parameters
are "height" which denotes height of the middle and "slope" which denotes
how sloped the plane is in each of the directions. Positive number of a
"slope" vector cause the corresponding direction to slope up as the
coordinate increases.

PowerGenerator represents a geometric function with an additional parameter
called "power". This parameter tells the function how the slope changes over
the course. Power of 2 causes the slope to become steeper as the coordinate
increases.
