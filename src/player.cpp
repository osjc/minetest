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

/*
(c) 2010 Perttu Ahola <celeron55@gmail.com>
*/

#include "player.h"
#include "map.h"
#include "connection.h"
#include "constants.h"

Player::Player():
	touching_ground(false),
	inventory(PLAYER_INVENTORY_SIZE),
	peer_id(PEER_ID_NEW),
	m_speed(0,0,0),
	m_position(0,0,0)
{
	updateName("<not set>");
}

Player::~Player()
{
}

void Player::move(f32 dtime, Map &map)
{
	v3f position = getPosition();
	v3f oldpos = position;
	v3s16 oldpos_i = floatToInt(oldpos);

	/*std::cout<<"oldpos_i=("<<oldpos_i.X<<","<<oldpos_i.Y<<","
			<<oldpos_i.Z<<")"<<std::endl;*/

	position += m_speed * dtime;

	// Skip collision detection if player is non-local
	if(isLocal() == false)
	{
		setPosition(position);
		return;
	}

	/*
		Collision detection
	*/

	// The frame length is limited to the player going 0.1*BS per call
	f32 d = (float)BS * 0.15;

#define PLAYER_RADIUS (BS*0.3)
#define PLAYER_HEIGHT (BS*1.7)

	core::aabbox3d<f32> playerbox(
		position.X - PLAYER_RADIUS,
		position.Y - 0.0,
		position.Z - PLAYER_RADIUS,
		position.X + PLAYER_RADIUS,
		position.Y + PLAYER_HEIGHT,
		position.Z + PLAYER_RADIUS
	);
	core::aabbox3d<f32> playerbox_old(
		oldpos.X - PLAYER_RADIUS,
		oldpos.Y - 0.0,
		oldpos.Z - PLAYER_RADIUS,
		oldpos.X + PLAYER_RADIUS,
		oldpos.Y + PLAYER_HEIGHT,
		oldpos.Z + PLAYER_RADIUS
	);

	//hilightboxes.push_back(playerbox);

	touching_ground = false;
	
	/*std::cout<<"Checking collisions for ("
			<<oldpos_i.X<<","<<oldpos_i.Y<<","<<oldpos_i.Z
			<<") -> ("
			<<pos_i.X<<","<<pos_i.Y<<","<<pos_i.Z
			<<"):"<<std::endl;*/

	for(s16 y = oldpos_i.Y - 1; y <= oldpos_i.Y + 2; y++){
		for(s16 z = oldpos_i.Z - 1; z <= oldpos_i.Z + 1; z++){
			for(s16 x = oldpos_i.X - 1; x <= oldpos_i.X + 1; x++){
				//std::cout<<"with ("<<x<<","<<y<<","<<z<<"): ";
				try{
					if(map.getNode(v3s16(x,y,z)).d == MATERIAL_AIR){
						//std::cout<<"air."<<std::endl;
						continue;
					}
				}
				catch(InvalidPositionException &e)
				{
					// Doing nothing here will block the player from
					// walking over map borders
				}

				core::aabbox3d<f32> nodebox = Map::getNodeBox(
						v3s16(x,y,z));
				
				// See if the player is touching ground
				if(
						fabs(nodebox.MaxEdge.Y-playerbox.MinEdge.Y) < d
						&& nodebox.MaxEdge.X-d > playerbox.MinEdge.X
						&& nodebox.MinEdge.X+d < playerbox.MaxEdge.X
						&& nodebox.MaxEdge.Z-d > playerbox.MinEdge.Z
						&& nodebox.MinEdge.Z+d < playerbox.MaxEdge.Z
				){
					touching_ground = true;
				}
				
				if(playerbox.intersectsWithBox(nodebox))
				{
				
	v3f dirs[3] = {
		v3f(0,0,1), // back
		v3f(0,1,0), // top
		v3f(1,0,0), // right
	};
	for(u16 i=0; i<3; i++)
	{
		f32 nodemax = nodebox.MaxEdge.dotProduct(dirs[i]);
		f32 nodemin = nodebox.MinEdge.dotProduct(dirs[i]);
		f32 playermax = playerbox.MaxEdge.dotProduct(dirs[i]);
		f32 playermin = playerbox.MinEdge.dotProduct(dirs[i]);
		f32 playermax_old = playerbox_old.MaxEdge.dotProduct(dirs[i]);
		f32 playermin_old = playerbox_old.MinEdge.dotProduct(dirs[i]);

		bool main_edge_collides = 
			((nodemax > playermin && nodemax <= playermin_old + d
				&& m_speed.dotProduct(dirs[i]) < 0)
			||
			(nodemin < playermax && nodemin >= playermax_old - d
				&& m_speed.dotProduct(dirs[i]) > 0));

		bool other_edges_collide = true;
		for(u16 j=0; j<3; j++)
		{
			if(j == i)
				continue;
			f32 nodemax = nodebox.MaxEdge.dotProduct(dirs[j]);
			f32 nodemin = nodebox.MinEdge.dotProduct(dirs[j]);
			f32 playermax = playerbox.MaxEdge.dotProduct(dirs[j]);
			f32 playermin = playerbox.MinEdge.dotProduct(dirs[j]);
			if(!(nodemax - d > playermin && nodemin + d < playermax))
			{
				other_edges_collide = false;
				break;
			}
		}
		
		if(main_edge_collides && other_edges_collide)
		{
			m_speed -= m_speed.dotProduct(dirs[i]) * dirs[i];
			position -= position.dotProduct(dirs[i]) * dirs[i];
			position += oldpos.dotProduct(dirs[i]) * dirs[i];
		}
	
	}
				} // if(playerbox.intersectsWithBox(nodebox))
			} // for x
		} // for z
	} // for y

	setPosition(position);
}

// Y direction is ignored
void Player::accelerate(v3f target_speed, f32 max_increase)
{
	if(m_speed.X < target_speed.X - max_increase)
		m_speed.X += max_increase;
	else if(m_speed.X > target_speed.X + max_increase)
		m_speed.X -= max_increase;
	else if(m_speed.X < target_speed.X)
		m_speed.X = target_speed.X;
	else if(m_speed.X > target_speed.X)
		m_speed.X = target_speed.X;

	if(m_speed.Z < target_speed.Z - max_increase)
		m_speed.Z += max_increase;
	else if(m_speed.Z > target_speed.Z + max_increase)
		m_speed.Z -= max_increase;
	else if(m_speed.Z < target_speed.Z)
		m_speed.Z = target_speed.Z;
	else if(m_speed.Z > target_speed.Z)
		m_speed.Z = target_speed.Z;
}

/*
	RemotePlayer
*/

RemotePlayer::RemotePlayer(
		scene::ISceneNode* parent,
		IrrlichtDevice *device,
		s32 id):
	scene::ISceneNode(parent, (device==NULL)?NULL:device->getSceneManager(), id),
	m_text(NULL)
{
	m_box = core::aabbox3d<f32>(-BS/2,0,-BS/2,BS/2,BS*2,BS/2);
	
	if(parent != NULL && device != NULL)
	{
		// ISceneNode stores a member called SceneManager
		scene::ISceneManager* mgr = SceneManager;
		video::IVideoDriver* driver = mgr->getVideoDriver();
		gui::IGUIEnvironment* gui = device->getGUIEnvironment();

		// Add a text node for showing the name
		wchar_t wname[1] = {0};
		m_text = mgr->addTextSceneNode(gui->getBuiltInFont(),
				wname, video::SColor(255,255,255,255), this);
		m_text->setPosition(v3f(0, (f32)BS*2.1, 0));

		// Attach a simple mesh to the player for showing an image
		scene::SMesh *mesh = new scene::SMesh();
		{ // Front
		scene::IMeshBuffer *buf = new scene::SMeshBuffer();
		video::SColor c(255,255,255,255);
		video::S3DVertex vertices[4] =
		{
			video::S3DVertex(-BS/2,0,0, 0,0,0, c, 0,1),
			video::S3DVertex(BS/2,0,0, 0,0,0, c, 1,1),
			video::S3DVertex(BS/2,BS*2,0, 0,0,0, c, 1,0),
			video::S3DVertex(-BS/2,BS*2,0, 0,0,0, c, 0,0),
		};
		u16 indices[] = {0,1,2,2,3,0};
		buf->append(vertices, 4, indices, 6);
		// Set material
		buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
		//buf->getMaterial().setFlag(video::EMF_BACK_FACE_CULLING, false);
		buf->getMaterial().setTexture(0, driver->getTexture("../data/player.png"));
		buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
		//buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		// Add to mesh
		mesh->addMeshBuffer(buf);
		buf->drop();
		}
		{ // Back
		scene::IMeshBuffer *buf = new scene::SMeshBuffer();
		video::SColor c(255,255,255,255);
		video::S3DVertex vertices[4] =
		{
			video::S3DVertex(BS/2,0,0, 0,0,0, c, 1,1),
			video::S3DVertex(-BS/2,0,0, 0,0,0, c, 0,1),
			video::S3DVertex(-BS/2,BS*2,0, 0,0,0, c, 0,0),
			video::S3DVertex(BS/2,BS*2,0, 0,0,0, c, 1,0),
		};
		u16 indices[] = {0,1,2,2,3,0};
		buf->append(vertices, 4, indices, 6);
		// Set material
		buf->getMaterial().setFlag(video::EMF_LIGHTING, false);
		//buf->getMaterial().setFlag(video::EMF_BACK_FACE_CULLING, false);
		buf->getMaterial().setTexture(0, driver->getTexture("../data/player_back.png"));
		buf->getMaterial().setFlag(video::EMF_BILINEAR_FILTER, false);
		buf->getMaterial().MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
		// Add to mesh
		mesh->addMeshBuffer(buf);
		buf->drop();
		}
		scene::IMeshSceneNode *node = mgr->addMeshSceneNode(mesh, this);
		mesh->drop();
		node->setPosition(v3f(0,0,0));
	}
}

RemotePlayer::~RemotePlayer()
{
	if(SceneManager != NULL)
		ISceneNode::remove();
}

void RemotePlayer::updateName(const char *name)
{
	Player::updateName(name);
	if(m_text != NULL)
	{
		wchar_t wname[PLAYERNAME_SIZE];
		mbstowcs(wname, m_name, strlen(m_name)+1);
		m_text->setText(wname);
	}
}

/*
	LocalPlayer
*/

LocalPlayer::LocalPlayer()
{
}

LocalPlayer::~LocalPlayer()
{
}

void LocalPlayer::applyControl(float dtime)
{
	// Random constants
#define WALK_ACCELERATION (4.0 * BS)
#define WALKSPEED_MAX (4.0 * BS)
	f32 walk_acceleration = WALK_ACCELERATION;
	f32 walkspeed_max = WALKSPEED_MAX;
	
	setPitch(control.pitch);
	setYaw(control.yaw);
	
	v3f move_direction = v3f(0,0,1);
	move_direction.rotateXZBy(getYaw());
	
	v3f speed = v3f(0,0,0);

	// Superspeed mode
	bool superspeed = false;
	if(control.superspeed)
	{
		speed += move_direction;
		superspeed = true;
	}

	if(control.up)
	{
		speed += move_direction;
	}
	if(control.down)
	{
		speed -= move_direction;
	}
	if(control.left)
	{
		speed += move_direction.crossProduct(v3f(0,1,0));
	}
	if(control.right)
	{
		speed += move_direction.crossProduct(v3f(0,-1,0));
	}
	if(control.jump)
	{
		if(touching_ground){
			v3f speed = getSpeed();
			speed.Y = 6.5*BS;
			setSpeed(speed);
		}
	}

	// The speed of the player (Y is ignored)
	if(superspeed)
		speed = speed.normalize() * walkspeed_max * 5;
	else
		speed = speed.normalize() * walkspeed_max;
	
	f32 inc = walk_acceleration * BS * dtime;
	
	// Accelerate to target speed with maximum increment
	accelerate(speed, inc);
}


