/*
Minetest-c55
Copyright (C) 2010 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef MAPNODE_HEADER
#define MAPNODE_HEADER

#include <iostream>
#include "common_irrlicht.h"
#include "light.h"
#include "utility.h"
#include "exceptions.h"
#include "serialization.h"

// Size of node in rendering units
#define BS 10

#define MATERIALS_COUNT 256

/*
	Ignored node.

	param is used for custom information in special containers,
	like VoxelManipulator.

	Anything that stores MapNodes doesn't have to preserve parameters
	associated with this material.
	
	Doesn't create faces with anything and is considered being
	out-of-map in the game map.
*/
#define MATERIAL_IGNORE 255
#define MATERIAL_IGNORE_DEFAULT_PARAM 0

/*
	The common material through which the player can walk and which
	is transparent to light
*/
#define MATERIAL_AIR 254

/*
	Materials-todo:

	GRAVEL
	- Dynamics of gravel: if there is a drop of more than two
	  blocks on any side, it will drop in there. Is this doable?
*/

enum Material
{
	MATERIAL_STONE=0,

	MATERIAL_GRASS,

	/*
		For water, the param is water pressure. 0...127.
		TODO: No, at least the lowest nibble is used for lighting.
		
		- Water will be a bit like light, but with different flow
		  behavior.
		- Water blocks will fall down if there is empty space below.
		- If there is water below, the pressure of the block below is
		  the pressure of the current block + 1, or higher.
		- If there is any pressure in a horizontally neighboring
		  block, a water block will try to move away from it.
		- If there is >=2 of pressure in a block below, water will
		  try to move upwards.
		- NOTE: To keep large operations fast, we have to keep a
		        cache of the water-air-surfaces, just like with light
	*/
	MATERIAL_WATER,

	MATERIAL_LIGHT,

	MATERIAL_TREE,
	MATERIAL_LEAVES,

	MATERIAL_GRASS_FOOTSTEPS,
	
	MATERIAL_MESE,

	MATERIAL_MUD,
	
	// This is set to the number of the actual values in this enum
	USEFUL_MATERIAL_COUNT
};

/*
	If true, the material allows light propagation and brightness is stored
	in param.
*/
inline bool light_propagates_material(u8 m)
{
	return (m == MATERIAL_AIR || m == MATERIAL_LIGHT || m == MATERIAL_WATER);
}

/*
	If true, the material allows lossless sunlight propagation.
*/
inline bool sunlight_propagates_material(u8 m)
{
	return (m == MATERIAL_AIR);
}

/*
	On a node-node surface, the material of the node with higher solidness
	is used for drawing.
	0: Invisible
	1: Transparent
	2: Opaque
*/
inline u8 material_solidness(u8 m)
{
	if(m == MATERIAL_AIR)
		return 0;
	if(m == MATERIAL_WATER)
		return 1;
	return 2;
}

/*
	Nodes make a face if materials differ and solidness differs.
	Return value:
		0: No face
		1: Face uses m1's material
		2: Face uses m2's material
*/
inline u8 face_materials(u8 m1, u8 m2)
{
	if(m1 == MATERIAL_IGNORE || m2 == MATERIAL_IGNORE)
		return 0;
	
	bool materials_differ = (m1 != m2);
	bool solidness_differs = (material_solidness(m1) != material_solidness(m2));
	bool makes_face = materials_differ && solidness_differs;

	if(makes_face == false)
		return 0;

	if(material_solidness(m1) > material_solidness(m2))
		return 1;
	else
		return 2;
}

/*
	Returns true for materials that form the base ground that
	follows the main heightmap
*/
inline bool is_ground_material(u8 m)
{
	return(
		m == MATERIAL_STONE ||
		m == MATERIAL_GRASS ||
		m == MATERIAL_GRASS_FOOTSTEPS ||
		m == MATERIAL_MESE ||
		m == MATERIAL_MUD
	);
}

struct MapNode
{
	//TODO: block type to differ from material
	//      (e.g. grass edges or something)
	// block type
	u8 d;

	/*
		Misc parameter. Initialized to 0.
		- For light_propagates() blocks, this is light intensity,
		  stored logarithmically from 0 to LIGHT_MAX.
		  Sunlight is LIGHT_SUN, which is LIGHT_MAX+1.
	*/
	s8 param;

	MapNode(const MapNode & n)
	{
		*this = n;
	}
	
	MapNode(u8 data=MATERIAL_AIR, u8 a_param=0)
	{
		d = data;
		param = a_param;
	}

	bool operator==(const MapNode &other)
	{
		return (d == other.d && param == other.param);
	}

	bool light_propagates()
	{
		return light_propagates_material(d);
	}
	
	bool sunlight_propagates()
	{
		return sunlight_propagates_material(d);
	}
	
	u8 solidness()
	{
		return material_solidness(d);
	}

	u8 light_source()
	{
		/*
			Note that a block that isn't light_propagates() can be a light source.
		*/
		if(d == MATERIAL_LIGHT)
			return LIGHT_MAX;
		
		return 0;
	}

	u8 getLight()
	{
		// Select the brightest of [light_source, transparent_light]
		u8 light = 0;
		if(light_propagates())
			light = param & 0x0f;
		if(light_source() > light)
			light = light_source();
		return light;
	}

	void setLight(u8 a_light)
	{
		// If not transparent, can't set light
		if(light_propagates() == false)
			return;
		param = a_light;
	}

	static u32 serializedLength(u8 version)
	{
		if(!ser_ver_supported(version))
			throw VersionMismatchException("ERROR: MapNode format not supported");
			
		if(version == 0)
			return 1;
		else
			return 2;
	}
	void serialize(u8 *dest, u8 version)
	{
		if(!ser_ver_supported(version))
			throw VersionMismatchException("ERROR: MapNode format not supported");
			
		if(version == 0)
		{
			dest[0] = d;
		}
		else
		{
			dest[0] = d;
			dest[1] = param;
		}
	}
	void deSerialize(u8 *source, u8 version)
	{
		if(!ser_ver_supported(version))
			throw VersionMismatchException("ERROR: MapNode format not supported");
			
		if(version == 0)
		{
			d = source[0];
		}
		else if(version == 1)
		{
			d = source[0];
			// This version doesn't support saved lighting
			if(light_propagates() || light_source() > 0)
				param = 0;
			else
				param = source[1];
		}
		else
		{
			d = source[0];
			param = source[1];
		}
	}
};

/*
	Returns integer position of the node in given
	floating point position.
*/
inline v3s16 floatToInt(v3f p)
{
	v3s16 p2(
		(p.X + (p.X>0 ? BS/2 : -BS/2))/BS,
		(p.Y + (p.Y>0 ? BS/2 : -BS/2))/BS,
		(p.Z + (p.Z>0 ? BS/2 : -BS/2))/BS);
	return p2;
}

inline v3f intToFloat(v3s16 p)
{
	v3f p2(
		p.X * BS,
		p.Y * BS,
		p.Z * BS
	);
	return p2;
}



#endif

