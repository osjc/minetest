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

#include "mapblock.h"
#include "map.h"
// For g_materials
#include "main.h"
#include "light.h"
#include <sstream>


/*
	MapBlock
*/

bool MapBlock::isValidPositionParent(v3s16 p)
{
	if(isValidPosition(p))
	{
		return true;
	}
	else{
		return m_parent->isValidPosition(getPosRelative() + p);
	}
}

MapNode MapBlock::getNodeParent(v3s16 p)
{
	if(isValidPosition(p) == false)
	{
		return m_parent->getNode(getPosRelative() + p);
	}
	else
	{
		if(data == NULL)
			throw InvalidPositionException();
		return data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X];
	}
}

void MapBlock::setNodeParent(v3s16 p, MapNode & n)
{
	if(isValidPosition(p) == false)
	{
		m_parent->setNode(getPosRelative() + p, n);
	}
	else
	{
		if(data == NULL)
			throw InvalidPositionException();
		data[p.Z*MAP_BLOCKSIZE*MAP_BLOCKSIZE + p.Y*MAP_BLOCKSIZE + p.X] = n;
	}
}

FastFace * MapBlock::makeFastFace(u8 material, u8 light, v3f p,
		v3f dir, v3f scale, v3f posRelative_f)
{
	FastFace *f = new FastFace;
	
	// Position is at the center of the cube.
	v3f pos = p * BS;
	posRelative_f *= BS;

	v3f vertex_pos[4];
	// If looking towards z+, this is the face that is behind
	// the center point, facing towards z+.
	vertex_pos[0] = v3f( BS/2,-BS/2,BS/2);
	vertex_pos[1] = v3f(-BS/2,-BS/2,BS/2);
	vertex_pos[2] = v3f(-BS/2, BS/2,BS/2);
	vertex_pos[3] = v3f( BS/2, BS/2,BS/2);
	
	/*
		TODO: Rotate it the right way (one side comes upside down)
	*/
	core::CMatrix4<f32> m;
	m.buildRotateFromTo(v3f(0,0,1), dir);
	
	for(u16 i=0; i<4; i++){
		m.rotateVect(vertex_pos[i]);
		vertex_pos[i].X *= scale.X;
		vertex_pos[i].Y *= scale.Y;
		vertex_pos[i].Z *= scale.Z;
		vertex_pos[i] += pos + posRelative_f;
	}

	f32 abs_scale = 1.;
	if     (scale.X < 0.999 || scale.X > 1.001) abs_scale = scale.X;
	else if(scale.Y < 0.999 || scale.Y > 1.001) abs_scale = scale.Y;
	else if(scale.Z < 0.999 || scale.Z > 1.001) abs_scale = scale.Z;

	v3f zerovector = v3f(0,0,0);
	
	u8 li = decode_light(light);
	//u8 li = 150;

	u8 alpha = 255;

	if(material == MATERIAL_WATER)
	{
		alpha = 128;
	}

	video::SColor c = video::SColor(alpha,li,li,li);

	/*f->vertices[0] = video::S3DVertex(vertex_pos[0], zerovector, c,
			core::vector2d<f32>(0,1));
	f->vertices[1] = video::S3DVertex(vertex_pos[1], zerovector, c,
			core::vector2d<f32>(abs_scale,1));
	f->vertices[2] = video::S3DVertex(vertex_pos[2], zerovector, c,
			core::vector2d<f32>(abs_scale,0));
	f->vertices[3] = video::S3DVertex(vertex_pos[3], zerovector, c,
			core::vector2d<f32>(0,0));*/
	f->vertices[0] = video::S3DVertex(vertex_pos[0], zerovector, c,
			core::vector2d<f32>(0,1));
	f->vertices[1] = video::S3DVertex(vertex_pos[1], zerovector, c,
			core::vector2d<f32>(abs_scale,1));
	f->vertices[2] = video::S3DVertex(vertex_pos[2], zerovector, c,
			core::vector2d<f32>(abs_scale,0));
	f->vertices[3] = video::S3DVertex(vertex_pos[3], zerovector, c,
			core::vector2d<f32>(0,0));

	f->material = material;

	return f;
}
	
/*
	Parameters must consist of air and !air.
	Order doesn't matter.

	If either of the nodes doesn't exist, light is 0.
*/
u8 MapBlock::getFaceLight(v3s16 p, v3s16 face_dir)
{
	try{
		MapNode n = getNodeParent(p);
		MapNode n2 = getNodeParent(p + face_dir);
		u8 light;
		if(n.solidness() < n2.solidness())
			light = n.getLight();
		else
			light = n2.getLight();

		// Make some nice difference to different sides
		if(face_dir.X == 1 || face_dir.Z == 1 || face_dir.Y == -1)
			light = diminish_light(diminish_light(light));
		else if(face_dir.X == -1 || face_dir.Z == -1)
			light = diminish_light(light);

		return light;
	}
	catch(InvalidPositionException &e)
	{
		return 0;
	}
}

/*
	Gets node material from any place relative to block.
	Returns MATERIAL_AIR if doesn't exist.
*/
u8 MapBlock::getNodeMaterial(v3s16 p)
{
	try{
		MapNode n = getNodeParent(p);
		return n.d;
	}
	catch(InvalidPositionException &e)
	{
		return MATERIAL_IGNORE;
	}
}

/*
	startpos:
	translate_dir: unit vector with only one of x, y or z
	face_dir: unit vector with only one of x, y or z
*/
void MapBlock::updateFastFaceRow(v3s16 startpos,
		u16 length,
		v3s16 translate_dir,
		v3s16 face_dir,
		core::list<FastFace*> &dest)
{
	/*
		Precalculate some variables
	*/
	v3f translate_dir_f(translate_dir.X, translate_dir.Y,
			translate_dir.Z); // floating point conversion
	v3f face_dir_f(face_dir.X, face_dir.Y,
			face_dir.Z); // floating point conversion
	v3f posRelative_f(getPosRelative().X, getPosRelative().Y,
			getPosRelative().Z); // floating point conversion

	v3s16 p = startpos;
	/*
		The light in the air lights the surface is taken from
		the node that is air.
	*/
	u8 light = getFaceLight(p, face_dir);
	
	u16 continuous_materials_count = 0;
	
	u8 material0 = getNodeMaterial(p);
	u8 material1 = getNodeMaterial(p + face_dir);
		
	for(u16 j=0; j<length; j++)
	{
		bool next_is_different = true;
		
		v3s16 p_next;
		u8 material0_next = 0;
		u8 material1_next = 0;
		u8 light_next = 0;

		if(j != length - 1){
			p_next = p + translate_dir;
			material0_next = getNodeMaterial(p_next);
			material1_next = getNodeMaterial(p_next + face_dir);
			light_next = getFaceLight(p_next, face_dir);

			if(material0_next == material0
					&& material1_next == material1
					&& light_next == light)
			{
				next_is_different = false;
			}
		}

		continuous_materials_count++;
		
		if(next_is_different)
		{
			/*
				Create a face if there should be one
			*/
			u8 mf = face_materials(material0, material1);
			
			if(mf != 0)
			{
				// Floating point conversion of the position vector
				v3f pf(p.X, p.Y, p.Z);
				// Center point of face (kind of)
				v3f sp = pf - ((f32)continuous_materials_count / 2. - 0.5) * translate_dir_f;
				v3f scale(1,1,1);
				if(translate_dir.X != 0){
					scale.X = continuous_materials_count;
				}
				if(translate_dir.Y != 0){
					scale.Y = continuous_materials_count;
				}
				if(translate_dir.Z != 0){
					scale.Z = continuous_materials_count;
				}
				
				FastFace *f;

				// If node at sp (material0) is more solid
				if(mf == 1)
				{
					f = makeFastFace(material0, light,
							sp, face_dir_f, scale,
							posRelative_f);
				}
				// If node at sp is less solid (mf == 2)
				else
				{
					f = makeFastFace(material1, light,
							sp+face_dir_f, -1*face_dir_f, scale,
							posRelative_f);
				}
				dest.push_back(f);
			}

			continuous_materials_count = 0;
			material0 = material0_next;
			material1 = material1_next;
			light = light_next;
		}
		
		p = p_next;
	}
}

/*
	This is used because CMeshBuffer::append() is very slow
*/
struct PreMeshBuffer
{
	video::SMaterial material;
	core::array<u16> indices;
	core::array<video::S3DVertex> vertices;
};

class MeshCollector
{
public:
	void append(
			video::SMaterial material,
			const video::S3DVertex* const vertices,
			u32 numVertices,
			const u16* const indices,
			u32 numIndices
		)
	{
		PreMeshBuffer *p = NULL;
		for(u32 i=0; i<m_prebuffers.size(); i++)
		{
			PreMeshBuffer &pp = m_prebuffers[i];
			if(pp.material != material)
				continue;

			p = &pp;
			break;
		}

		if(p == NULL)
		{
			PreMeshBuffer pp;
			pp.material = material;
			m_prebuffers.push_back(pp);
			p = &m_prebuffers[m_prebuffers.size()-1];
		}

		u32 vertex_count = p->vertices.size();
		for(u32 i=0; i<numIndices; i++)
		{
			u32 j = indices[i] + vertex_count;
			if(j > 65535)
			{
				dstream<<"FIXME: Meshbuffer ran out of indices"<<std::endl;
				// NOTE: Fix is to just add an another MeshBuffer
			}
			p->indices.push_back(j);
		}
		for(u32 i=0; i<numVertices; i++)
		{
			p->vertices.push_back(vertices[i]);
		}
	}

	void fillMesh(scene::SMesh *mesh)
	{
		/*dstream<<"Filling mesh with "<<m_prebuffers.size()
				<<" meshbuffers"<<std::endl;*/
		for(u32 i=0; i<m_prebuffers.size(); i++)
		{
			PreMeshBuffer &p = m_prebuffers[i];

			/*dstream<<"p.vertices.size()="<<p.vertices.size()
					<<", p.indices.size()="<<p.indices.size()
					<<std::endl;*/
			
			// Create meshbuffer
			
			// This is a "Standard MeshBuffer",
			// it's a typedeffed CMeshBuffer<video::S3DVertex>
			scene::SMeshBuffer *buf = new scene::SMeshBuffer();
			// Set material
			buf->Material = p.material;
			//((scene::SMeshBuffer*)buf)->Material = p.material;
			// Use VBO
			//buf->setHardwareMappingHint(scene::EHM_STATIC);
			// Add to mesh
			mesh->addMeshBuffer(buf);
			// Mesh grabbed it
			buf->drop();

			buf->append(p.vertices.pointer(), p.vertices.size(),
					p.indices.pointer(), p.indices.size());
		}
	}

private:
	core::array<PreMeshBuffer> m_prebuffers;
};

void MapBlock::updateMesh()
{
	/*v3s16 p = getPosRelative();
	std::cout<<"MapBlock("<<p.X<<","<<p.Y<<","<<p.Z<<")"
			<<"::updateMesh(): ";*/
			//<<"::updateMesh()"<<std::endl;
	
	/*
		TODO: Change this to directly generate the mesh (and get rid
		      of FastFaces)
	*/

	core::list<FastFace*> *fastfaces_new = new core::list<FastFace*>;
	
	/*
		We are including the faces of the trailing edges of the block.
		This means that when something changes, the caller must
		also update the meshes of the blocks at the leading edges.
	*/

	/*
		Go through every y,z and get top faces in rows of x+
	*/
	for(s16 y=0; y<MAP_BLOCKSIZE; y++){
	//for(s16 y=-1; y<MAP_BLOCKSIZE; y++){
		for(s16 z=0; z<MAP_BLOCKSIZE; z++){
			updateFastFaceRow(v3s16(0,y,z), MAP_BLOCKSIZE,
					v3s16(1,0,0),
					v3s16(0,1,0),
					*fastfaces_new);
		}
	}
	/*
		Go through every x,y and get right faces in rows of z+
	*/
	for(s16 x=0; x<MAP_BLOCKSIZE; x++){
	//for(s16 x=-1; x<MAP_BLOCKSIZE; x++){
		for(s16 y=0; y<MAP_BLOCKSIZE; y++){
			updateFastFaceRow(v3s16(x,y,0), MAP_BLOCKSIZE,
					v3s16(0,0,1),
					v3s16(1,0,0),
					*fastfaces_new);
		}
	}
	/*
		Go through every y,z and get back faces in rows of x+
	*/
	for(s16 z=0; z<MAP_BLOCKSIZE; z++){
	//for(s16 z=-1; z<MAP_BLOCKSIZE; z++){
		for(s16 y=0; y<MAP_BLOCKSIZE; y++){
			updateFastFaceRow(v3s16(0,y,z), MAP_BLOCKSIZE,
					v3s16(1,0,0),
					v3s16(0,0,1),
					*fastfaces_new);
		}
	}

	scene::SMesh *mesh_new = NULL;
	
	if(fastfaces_new->getSize() > 0)
	{
		MeshCollector collector;

		core::list<FastFace*>::Iterator i = fastfaces_new->begin();

		for(; i != fastfaces_new->end(); i++)
		{
			FastFace *f = *i;

			const u16 indices[] = {0,1,2,2,3,0};

			collector.append(g_materials[f->material], f->vertices, 4,
					indices, 6);
		}

		mesh_new = new scene::SMesh();
		
		collector.fillMesh(mesh_new);

#if 0
		scene::IMeshBuffer *buf = NULL;

		core::list<FastFace*>::Iterator i = fastfaces_new->begin();

		// MATERIAL_AIR shouldn't be used by any face
		u8 material_in_use = MATERIAL_AIR;

		for(; i != fastfaces_new->end(); i++)
		{
			FastFace *f = *i;
			
			if(f->material != material_in_use || buf == NULL)
			{
				// Try to get a meshbuffer associated with the material
				buf = mesh_new->getMeshBuffer(g_materials[f->material]);
				// If not found, create one
				if(buf == NULL)
				{
					// This is a "Standard MeshBuffer",
					// it's a typedeffed CMeshBuffer<video::S3DVertex>
					buf = new scene::SMeshBuffer();
					// Set material
					((scene::SMeshBuffer*)buf)->Material = g_materials[f->material];
					// Use VBO
					//buf->setHardwareMappingHint(scene::EHM_STATIC);
					// Add to mesh
					mesh_new->addMeshBuffer(buf);
					// Mesh grabbed it
					buf->drop();
				}
				material_in_use = f->material;
			}
			
			u16 new_indices[] = {0,1,2,2,3,0};
			
			//buf->append(f->vertices, 4, indices, 6);
		}
#endif

		// Use VBO for mesh (this just would set this for ever buffer)
		//mesh_new->setHardwareMappingHint(scene::EHM_STATIC);
		
		/*std::cout<<"MapBlock has "<<fastfaces_new->getSize()<<" faces "
				<<"and uses "<<mesh_new->getMeshBufferCount()
				<<" materials (meshbuffers)"<<std::endl;*/
	}

	// TODO: Get rid of the FastFace stage
	core::list<FastFace*>::Iterator i;
	i = fastfaces_new->begin();
	for(; i != fastfaces_new->end(); i++)
	{
		delete *i;
	}
	fastfaces_new->clear();
	delete fastfaces_new;

	/*
		Replace the mesh
	*/

	mesh_mutex.Lock();

	scene::SMesh *mesh_old = mesh;

	mesh = mesh_new;
	
	if(mesh_old != NULL)
	{
		// Remove hardware buffers of meshbuffers of mesh
		// NOTE: No way, this runs in a different thread and everything
		/*u32 c = mesh_old->getMeshBufferCount();
		for(u32 i=0; i<c; i++)
		{
			IMeshBuffer *buf = mesh_old->getMeshBuffer(i);
		}*/
		// Drop the mesh
		mesh_old->drop();
		//delete mesh_old;
	}

	mesh_mutex.Unlock();
	
	//std::cout<<"added "<<fastfaces.getSize()<<" faces."<<std::endl;
}

/*
	Propagates sunlight down through the block.
	Doesn't modify nodes that are not affected by sunlight.
	
	Returns false if sunlight at bottom block is invalid
	Returns true if bottom block doesn't exist.

	If there is a block above, continues from it.
	If there is no block above, assumes there is sunlight, unless
	is_underground is set.

	At the moment, all sunlighted nodes are added to light_sources.
	TODO: This could be optimized.
*/
bool MapBlock::propagateSunlight(core::map<v3s16, bool> & light_sources)
{
	// Whether the sunlight at the top of the bottom block is valid
	bool block_below_is_valid = true;
	
	v3s16 pos_relative = getPosRelative();
	
	for(s16 x=0; x<MAP_BLOCKSIZE; x++)
	{
		for(s16 z=0; z<MAP_BLOCKSIZE; z++)
		{
			bool no_sunlight = false;
			// Check if node above block has sunlight
			try{
				MapNode n = getNodeParent(v3s16(x, MAP_BLOCKSIZE, z));
				if(n.getLight() != LIGHT_SUN)
				{
					/*if(is_underground)
					{
						no_sunlight = true;
					}*/
					no_sunlight = true;
				}
			}
			catch(InvalidPositionException &e)
			{
				// TODO: This makes over-ground roofed places sunlighted
				// Assume sunlight, unless is_underground==true
				if(is_underground)
				{
					no_sunlight = true;
				}
				
				// TODO: There has to be some way to allow this behaviour
				// As of now, it just makes everything dark.
				// No sunlight here
				//no_sunlight = true;
			}

			/*std::cout<<"("<<x<<","<<z<<"): "
					<<"no_top_block="<<no_top_block
					<<", is_underground="<<is_underground
					<<", no_sunlight="<<no_sunlight
					<<std::endl;*/
		
			s16 y = MAP_BLOCKSIZE-1;
			
			if(no_sunlight == false)
			{
				// Continue spreading sunlight downwards through transparent
				// nodes
				for(; y >= 0; y--)
				{
					v3s16 pos(x, y, z);
					
					MapNode &n = getNodeRef(pos);

					if(n.sunlight_propagates())
					{
						n.setLight(LIGHT_SUN);

						light_sources.insert(pos_relative + pos, true);
					}
					else{
						break;
					}
				}
			}

			bool sunlight_should_go_down = (y==-1);

			// Fill rest with black (only transparent ones)
			for(; y >= 0; y--){
				v3s16 pos(x, y, z);
				
				MapNode &n = getNodeRef(pos);

				if(n.light_propagates())
				{
					n.setLight(0);
				}
				else{
					break;
				}
			}

			/*
				If the block below hasn't already been marked invalid:

				Check if the node below the block has proper sunlight at top.
				If not, the block below is invalid.
				
				Ignore non-transparent nodes as they always have no light
			*/
			try
			{
			if(block_below_is_valid)
			{
				MapNode n = getNodeParent(v3s16(x, -1, z));
				if(n.light_propagates())
				{
					if(n.getLight() == LIGHT_SUN
							&& sunlight_should_go_down == false)
						block_below_is_valid = false;
					else if(n.getLight() != LIGHT_SUN
							&& sunlight_should_go_down == true)
						block_below_is_valid = false;
				}
			}//if
			}//try
			catch(InvalidPositionException &e)
			{
				/*std::cout<<"InvalidBlockException for bottom block node"
						<<std::endl;*/
				// Just no block below, no need to panic.
			}
		}
	}

	return block_below_is_valid;
}

/*
	Serialization
*/

void MapBlock::serialize(std::ostream &os, u8 version)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");
	
	if(data == NULL)
	{
		throw SerializationError("ERROR: Not writing dummy block.");
	}
	
	// These have no compression
	if(version <= 3 || version == 5 || version == 6)
	{
		u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;
		
		u32 buflen = 1 + nodecount * MapNode::serializedLength(version);
		SharedBuffer<u8> dest(buflen);

		dest[0] = is_underground;
		for(u32 i=0; i<nodecount; i++)
		{
			u32 s = 1 + i * MapNode::serializedLength(version);
			data[i].serialize(&dest[s], version);
		}
		
		os.write((char*)*dest, dest.getSize());
	}
	// All otherversions
	else
	{
		/*
			With compression.
			Compress the materials and the params separately.
		*/
		
		// First byte
		os.write((char*)&is_underground, 1);

		u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;

		// Get and compress materials
		SharedBuffer<u8> materialdata(nodecount);
		for(u32 i=0; i<nodecount; i++)
		{
			materialdata[i] = data[i].d;
		}
		compress(materialdata, os, version);

		// Get and compress params
		SharedBuffer<u8> paramdata(nodecount);
		for(u32 i=0; i<nodecount; i++)
		{
			paramdata[i] = data[i].param;
		}
		compress(paramdata, os, version);
	}
}

void MapBlock::deSerialize(std::istream &is, u8 version)
{
	if(!ser_ver_supported(version))
		throw VersionMismatchException("ERROR: MapBlock format not supported");

	// These have no compression
	if(version <= 3 || version == 5 || version == 6)
	{
		u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;
		char tmp;
		is.read(&tmp, 1);
		if(is.gcount() != 1)
			throw SerializationError
					("MapBlock::deSerialize: no enough input data");
		is_underground = tmp;
		for(u32 i=0; i<nodecount; i++)
		{
			s32 len = MapNode::serializedLength(version);
			SharedBuffer<u8> d(len);
			is.read((char*)*d, len);
			if(is.gcount() != len)
				throw SerializationError
						("MapBlock::deSerialize: no enough input data");
			data[i].deSerialize(*d, version);
		}
	}
	// All other versions
	else
	{
		u32 nodecount = MAP_BLOCKSIZE*MAP_BLOCKSIZE*MAP_BLOCKSIZE;

		u8 t8;
		is.read((char*)&t8, 1);
		is_underground = t8;

		{
			// Uncompress and set material data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if(s.size() != nodecount)
				throw SerializationError
						("MapBlock::deSerialize: invalid format");
			for(u32 i=0; i<s.size(); i++)
			{
				data[i].d = s[i];
			}
		}
		{
			// Uncompress and set param data
			std::ostringstream os(std::ios_base::binary);
			decompress(is, os, version);
			std::string s = os.str();
			if(s.size() != nodecount)
				throw SerializationError
						("MapBlock::deSerialize: invalid format");
			for(u32 i=0; i<s.size(); i++)
			{
				data[i].param = s[i];
			}
		}
	}
}


//END
