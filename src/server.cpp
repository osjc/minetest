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

#include "server.h"
#include "utility.h"
#include <iostream>
#include "clientserver.h"
#include "map.h"
#include "jmutexautolock.h"
#include "main.h"
#include "constants.h"

void * ServerThread::Thread()
{
	ThreadStarted();

	while(getRun())
	{
		try{
			m_server->AsyncRunStep();
		
			//dout_server<<"Running m_server->Receive()"<<std::endl;
			m_server->Receive();
		}
		catch(con::NoIncomingDataException &e)
		{
		}
#if CATCH_UNHANDLED_EXCEPTIONS
		/*
			This is what has to be done in threads to get suitable debug info
		*/
		catch(std::exception &e)
		{
			dstream<<std::endl<<DTIME<<"An unhandled exception occurred: "
					<<e.what()<<std::endl;
			assert(0);
		}
#endif
	}
	

	return NULL;
}

void * EmergeThread::Thread()
{
	ThreadStarted();

	bool debug=false;
#if CATCH_UNHANDLED_EXCEPTIONS
	try
	{
#endif
	
	/*
		Get block info from queue, emerge them and send them
		to clients.

		After queue is empty, exit.
	*/
	while(getRun())
	{
		QueuedBlockEmerge *qptr = m_server->m_emerge_queue.pop();
		if(qptr == NULL)
			break;
		
		SharedPtr<QueuedBlockEmerge> q(qptr);

		v3s16 &p = q->pos;
		
		//derr_server<<"EmergeThread::Thread(): running"<<std::endl;
		
		/*
			Try to emerge it from somewhere.

			If it is only wanted as optional, only loading from disk
			will be allowed.
		*/
		
		/*
			Check if any peer wants it as non-optional. In that case it
			will be generated.

			Also decrement the emerge queue count in clients.
		*/

		bool optional = true;

		{
			core::map<u16, u8>::Iterator i;
			for(i=q->peer_ids.getIterator(); i.atEnd()==false; i++)
			{
				//u16 peer_id = i.getNode()->getKey();

				// Check flags
				u8 flags = i.getNode()->getValue();
				if((flags & TOSERVER_GETBLOCK_FLAG_OPTIONAL) == false)
					optional = false;
				
			}
		}

		/*dstream<<"EmergeThread: p="
				<<"("<<p.X<<","<<p.Y<<","<<p.Z<<") "
				<<"optional="<<optional<<std::endl;*/
		
		ServerMap &map = ((ServerMap&)m_server->m_env.getMap());
			
		core::map<v3s16, MapBlock*> changed_blocks;
		core::map<v3s16, MapBlock*> lighting_invalidated_blocks;

		MapBlock *block = NULL;
		bool got_block = true;
		core::map<v3s16, MapBlock*> modified_blocks;
		
		{//envlock

		JMutexAutoLock envlock(m_server->m_env_mutex);

		//TimeTaker timer("block emerge envlock", g_device);
			
		try{
			bool only_from_disk = false;
			
			if(optional)
				only_from_disk = true;

			block = map.emergeBlock(
					p,
					only_from_disk,
					changed_blocks,
					lighting_invalidated_blocks);
			
			// If it is a dummy, block was not found on disk
			if(block->isDummy())
			{
				//dstream<<"EmergeThread: Got a dummy block"<<std::endl;
				got_block = false;
			}
		}
		catch(InvalidPositionException &e)
		{
			// Block not found.
			// This happens when position is over limit.
			got_block = false;
		}
		
		if(got_block)
		{
			if(debug && changed_blocks.size() > 0)
			{
				dout_server<<DTIME<<"Got changed_blocks: ";
				for(core::map<v3s16, MapBlock*>::Iterator i = changed_blocks.getIterator();
						i.atEnd() == false; i++)
				{
					MapBlock *block = i.getNode()->getValue();
					v3s16 p = block->getPos();
					dout_server<<"("<<p.X<<","<<p.Y<<","<<p.Z<<") ";
				}
				dout_server<<std::endl;
			}

			/*
				Collect a list of blocks that have been modified in
				addition to the fetched one.
			*/

			// Add all the "changed blocks"
			for(core::map<v3s16, MapBlock*>::Iterator i = changed_blocks.getIterator();
					i.atEnd() == false; i++)
			{
				MapBlock *block = i.getNode()->getValue();
				modified_blocks.insert(block->getPos(), block);
			}
			
			/*dstream<<"lighting "<<lighting_invalidated_blocks.size()
					<<" blocks"<<std::endl;
			TimeTaker timer("** updateLighting", g_device);*/
			
			// Update lighting without locking the environment mutex,
			// add modified blocks to changed blocks
			map.updateLighting(lighting_invalidated_blocks, modified_blocks);
		}
		// If we got no block, there should be no invalidated blocks
		else
		{
			assert(lighting_invalidated_blocks.size() == 0);
		}

		}//envlock

		/*
			Set sent status of modified blocks on clients
		*/
	
		// NOTE: Server's clients are also behind the connection mutex
		JMutexAutoLock lock(m_server->m_con_mutex);

		/*
			Add the originally fetched block to the modified list
		*/
		if(got_block)
		{
			modified_blocks.insert(p, block);
		}
		
		/*
			Set the modified blocks unsent for all the clients
		*/
		
		for(core::map<u16, RemoteClient*>::Iterator
				i = m_server->m_clients.getIterator();
				i.atEnd() == false; i++)
		{
			RemoteClient *client = i.getNode()->getValue();
			
			if(modified_blocks.size() > 0)
			{
				// Remove block from sent history
				client->SetBlocksNotSent(modified_blocks);
			}
			
			/*if(q->peer_ids.find(client->peer_id) != NULL)
			{
				// Decrement emerge queue count of client
				client->BlockEmerged();
			}*/
		}
		
	}
#if CATCH_UNHANDLED_EXCEPTIONS
	}//try
	/*
		This is what has to be done in threads to get suitable debug info
	*/
	catch(std::exception &e)
	{
		dstream<<std::endl<<DTIME<<"An unhandled exception occurred: "
				<<e.what()<<std::endl;
		assert(0);
	}
#endif

	return NULL;
}

void RemoteClient::GetNextBlocks(Server *server, float dtime,
		core::array<PrioritySortedBlockTransfer> &dest)
{
	// Won't send anything if already sending
	{
		JMutexAutoLock lock(m_blocks_sending_mutex);
		
		if(m_blocks_sending.size() >= g_settings.getU16
				("max_simultaneous_block_sends_per_client"))
		{
			//dstream<<"Not sending any blocks, Queue full."<<std::endl;
			return;
		}
	}

	Player *player = server->m_env.getPlayer(peer_id);

	v3f playerpos = player->getPosition();

	v3s16 center_nodepos = floatToInt(playerpos);

	v3s16 center = getNodeBlockPos(center_nodepos);

	/*
		Get the starting value of the block finder radius.
	*/
	s16 last_nearest_unsent_d;
	s16 d_start;
	{
		JMutexAutoLock lock(m_blocks_sent_mutex);
		
		if(m_last_center != center)
		{
			m_nearest_unsent_d = 0;
			m_last_center = center;
		}

		static float reset_counter = 0;
		reset_counter += dtime;
		if(reset_counter > 5.0)
		{
			reset_counter = 0;
			m_nearest_unsent_d = 0;
		}

		last_nearest_unsent_d = m_nearest_unsent_d;
		
		d_start = m_nearest_unsent_d;
	}

	u16 maximum_simultaneous_block_sends_setting = g_settings.getU16
			("max_simultaneous_block_sends_per_client");
	u16 maximum_simultaneous_block_sends = 
			maximum_simultaneous_block_sends_setting;

	/*
		Check the time from last addNode/removeNode.
		
		Decrease send rate if player is building stuff.
	*/
	{
		SharedPtr<JMutexAutoLock> lock(m_time_from_building.getLock());
		m_time_from_building.m_value += dtime;
		if(m_time_from_building.m_value
				< FULL_BLOCK_SEND_ENABLE_MIN_TIME_FROM_BUILDING)
		{
			maximum_simultaneous_block_sends
				= LIMITED_MAX_SIMULTANEOUS_BLOCK_SENDS;
		}
	}

	// Serialization version used
	//u8 ser_version = serialization_version;

	//bool has_incomplete_blocks = false;
	
	/*
		TODO: Get this from somewhere
	*/
	//s16 d_max = 7;
	s16 d_max = 8;

	//TODO: Get this from somewhere (probably a bigger value)
	s16 d_max_gen = 5;
	
	//dstream<<"Starting from "<<d_start<<std::endl;

	for(s16 d = d_start; d <= d_max; d++)
	{
		//dstream<<"RemoteClient::SendBlocks(): d="<<d<<std::endl;
		
		//if(has_incomplete_blocks == false)
		{
			JMutexAutoLock lock(m_blocks_sent_mutex);
			/*
				If m_nearest_unsent_d was changed by the EmergeThread
				(it can change it to 0 through SetBlockNotSent),
				update our d to it.
				Else update m_nearest_unsent_d
			*/
			if(m_nearest_unsent_d != last_nearest_unsent_d)
			{
				d = m_nearest_unsent_d;
			}
			else
			{
				m_nearest_unsent_d = d;
			}
			last_nearest_unsent_d = m_nearest_unsent_d;
		}

		/*
			Get the border/face dot coordinates of a "d-radiused"
			box
		*/
		core::list<v3s16> list;
		getFacePositions(list, d);
		
		core::list<v3s16>::Iterator li;
		for(li=list.begin(); li!=list.end(); li++)
		{
			v3s16 p = *li + center;
			
			/*
				Send throttling
				- Don't allow too many simultaneous transfers
				- EXCEPT when the blocks are very close

				Also, don't send blocks that are already flying.
			*/
			
			u16 maximum_simultaneous_block_sends_now =
					maximum_simultaneous_block_sends;
			
			if(d <= BLOCK_SEND_DISABLE_LIMITS_MAX_D)
					maximum_simultaneous_block_sends_now =
							maximum_simultaneous_block_sends_setting;

			{
				JMutexAutoLock lock(m_blocks_sending_mutex);
				
				// Limit is dynamically lowered when building
				if(m_blocks_sending.size()
						>= maximum_simultaneous_block_sends_now)
				{
					/*dstream<<"Not sending more blocks. Queue full. "
							<<m_blocks_sending.size()
							<<std::endl;*/
					return;
				}

				if(m_blocks_sending.find(p) != NULL)
					continue;
			}
			
			/*
				Do not go over-limit
			*/
			if(p.X < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.X > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Y < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Y > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Z < -MAP_GENERATION_LIMIT / MAP_BLOCKSIZE
			|| p.Z > MAP_GENERATION_LIMIT / MAP_BLOCKSIZE)
				continue;

			bool generate = d <= d_max_gen;
		
			// Limit the generating area vertically to half
			if(abs(p.Y - center.Y) > d_max_gen / 2)
				generate = false;
			
			/*
				Don't send already sent blocks
			*/
			{
				JMutexAutoLock lock(m_blocks_sent_mutex);
				
				if(m_blocks_sent.find(p) != NULL)
					continue;
			}
					
			/*
				Check if map has this block
			*/
			MapBlock *block = NULL;
			try
			{
				block = server->m_env.getMap().getBlockNoCreate(p);
			}
			catch(InvalidPositionException &e)
			{
			}
			
			bool surely_not_found_on_disk = false;
			if(block != NULL)
			{
				/*if(block->isIncomplete())
				{
					has_incomplete_blocks = true;
					continue;
				}*/

				if(block->isDummy())
				{
					surely_not_found_on_disk = true;
				}
			}

			/*
				If block has been marked to not exist on disk (dummy)
				and generating new ones is not wanted, skip block.
			*/
			if(generate == false && surely_not_found_on_disk == true)
			{
				// get next one.
				continue;
			}

			/*
				Add inexistent block to emerge queue.
			*/
			if(block == NULL || surely_not_found_on_disk)
			{
				/*SharedPtr<JMutexAutoLock> lock
						(m_num_blocks_in_emerge_queue.getLock());*/
				
				//TODO: Get value from somewhere
				// Allow only one block in emerge queue
				if(server->m_emerge_queue.peerItemCount(peer_id) < 1)
				{
					// Add it to the emerge queue and trigger the thread
					
					u8 flags = 0;
					if(generate == false)
						flags |= TOSERVER_GETBLOCK_FLAG_OPTIONAL;
					
					server->m_emerge_queue.addBlock(peer_id, p, flags);
					server->m_emergethread.trigger();
				}
				
				// get next one.
				continue;
			}

			/*
				Add block to queue
			*/

			PrioritySortedBlockTransfer q((float)d, p, peer_id);

			dest.push_back(q);
		}
	}

	// Don't add anything here. The loop breaks by returning.
}

void RemoteClient::SendObjectData(
		Server *server,
		float dtime,
		core::map<v3s16, bool> &stepped_blocks
	)
{
	// Can't send anything without knowing version
	if(serialization_version == SER_FMT_VER_INVALID)
	{
		dstream<<"RemoteClient::SendObjectData(): Not sending, no version."
				<<std::endl;
		return;
	}

	/*
		Send a TOCLIENT_OBJECTDATA packet.
		Sent as unreliable.

		u16 command
		u16 number of player positions
		for each player:
			v3s32 position*100
			v3s32 speed*100
			s32 pitch*100
			s32 yaw*100
		u16 count of blocks
		for each block:
			block objects
	*/

	std::ostringstream os(std::ios_base::binary);
	u8 buf[12];
	
	// Write command
	writeU16(buf, TOCLIENT_OBJECTDATA);
	os.write((char*)buf, 2);
	
	/*
		Get and write player data
	*/

	core::list<Player*> players = server->m_env.getPlayers();

	// Write player count
	u16 playercount = players.size();
	writeU16(buf, playercount);
	os.write((char*)buf, 2);

	core::list<Player*>::Iterator i;
	for(i = players.begin();
			i != players.end(); i++)
	{
		Player *player = *i;

		v3f pf = player->getPosition();
		v3f sf = player->getSpeed();

		v3s32 position_i(pf.X*100, pf.Y*100, pf.Z*100);
		v3s32 speed_i   (sf.X*100, sf.Y*100, sf.Z*100);
		s32   pitch_i   (player->getPitch() * 100);
		s32   yaw_i     (player->getYaw() * 100);
		
		writeU16(buf, player->peer_id);
		os.write((char*)buf, 2);
		writeV3S32(buf, position_i);
		os.write((char*)buf, 12);
		writeV3S32(buf, speed_i);
		os.write((char*)buf, 12);
		writeS32(buf, pitch_i);
		os.write((char*)buf, 4);
		writeS32(buf, yaw_i);
		os.write((char*)buf, 4);
	}
	
	/*
		Get and write object data
	*/

	/*
		Get nearby blocks.
		
		For making players to be able to build to their nearby
		environment (building is not possible on blocks that are not
		in memory):
		- Set blocks changed
		- Add blocks to emerge queue if they are not found

		SUGGESTION: These could be ignored from the backside of the player

		TODO: Keep track of total size of packet and stop when it is too big
	*/

	Player *player = server->m_env.getPlayer(peer_id);

	v3f playerpos = player->getPosition();

	v3s16 center_nodepos = floatToInt(playerpos);
	v3s16 center = getNodeBlockPos(center_nodepos);

	//s16 d_max = ACTIVE_OBJECT_D_BLOCKS;
	s16 d_max = g_settings.getS16("active_object_range");
	
	// Number of blocks whose objects were written to bos
	u16 blockcount = 0;

	//core::map<v3s16, MapBlock*> blocks;
	std::ostringstream bos(std::ios_base::binary);

	for(s16 d = 0; d <= d_max; d++)
	{
		core::list<v3s16> list;
		getFacePositions(list, d);
		
		core::list<v3s16>::Iterator li;
		for(li=list.begin(); li!=list.end(); li++)
		{
			v3s16 p = *li + center;

			/*
				Ignore blocks that haven't been sent to the client
			*/
			{
				JMutexAutoLock sentlock(m_blocks_sent_mutex);
				if(m_blocks_sent.find(p) == NULL)
					continue;
			}
			
			// Try stepping block and add it to a send queue
			try
			{

			// Get block
			MapBlock *block = server->m_env.getMap().getBlockNoCreate(p);

			// Skip block if there are no objects
			if(block->getObjectCount() == 0)
				continue;
			
			// Step block if not in stepped_blocks and add to stepped_blocks
			if(stepped_blocks.find(p) == NULL)
			{
				block->stepObjects(dtime, true);
				stepped_blocks.insert(p, true);
				block->setChangedFlag();
			}

			/*
				Write objects
			*/

			// Write blockpos
			writeV3S16(buf, p);
			bos.write((char*)buf, 6);

			// Write objects
			block->serializeObjects(bos, serialization_version);

			blockcount++;

			/*
				Stop collecting objects if data is already too big
			*/
			// Sum of player and object data sizes
			s32 sum = (s32)os.tellp() + 2 + (s32)bos.tellp();
			// break out if data too big
			if(sum > MAX_OBJECTDATA_SIZE)
			{
				goto skip_subsequent;
			}
			
			} //try
			catch(InvalidPositionException &e)
			{
				// Not in memory
				// Add it to the emerge queue and trigger the thread.
				// Fetch the block only if it is on disk.
				
				// Grab and increment counter
				/*SharedPtr<JMutexAutoLock> lock
						(m_num_blocks_in_emerge_queue.getLock());
				m_num_blocks_in_emerge_queue.m_value++;*/
				
				// Add to queue as an anonymous fetch from disk
				u8 flags = TOSERVER_GETBLOCK_FLAG_OPTIONAL;
				server->m_emerge_queue.addBlock(0, p, flags);
				server->m_emergethread.trigger();
			}
		}
	}

skip_subsequent:

	// Write block count
	writeU16(buf, blockcount);
	os.write((char*)buf, 2);

	// Write block objects
	os<<bos.str();

	/*
		Send data
	*/
	
	//dstream<<"Server: Sending object data to "<<peer_id<<std::endl;

	// Make data buffer
	std::string s = os.str();
	SharedBuffer<u8> data((u8*)s.c_str(), s.size());
	// Send as unreliable
	server->m_con.Send(peer_id, 0, data, false);
}

void RemoteClient::GotBlock(v3s16 p)
{
	JMutexAutoLock lock(m_blocks_sending_mutex);
	JMutexAutoLock lock2(m_blocks_sent_mutex);
	if(m_blocks_sending.find(p) != NULL)
		m_blocks_sending.remove(p);
	else
		dstream<<"RemoteClient::GotBlock(): Didn't find in"
				" m_blocks_sending"<<std::endl;
	m_blocks_sent.insert(p, true);
}

void RemoteClient::SentBlock(v3s16 p)
{
	JMutexAutoLock lock(m_blocks_sending_mutex);
	if(m_blocks_sending.size() > 15)
	{
		dstream<<"RemoteClient::SentBlock(): "
				<<"m_blocks_sending.size()="
				<<m_blocks_sending.size()<<std::endl;
	}
	if(m_blocks_sending.find(p) == NULL)
		m_blocks_sending.insert(p, 0.0);
	else
		dstream<<"RemoteClient::SentBlock(): Sent block"
				" already in m_blocks_sending"<<std::endl;
}

void RemoteClient::SetBlockNotSent(v3s16 p)
{
	JMutexAutoLock sendinglock(m_blocks_sending_mutex);
	JMutexAutoLock sentlock(m_blocks_sent_mutex);

	m_nearest_unsent_d = 0;
	
	if(m_blocks_sending.find(p) != NULL)
		m_blocks_sending.remove(p);
	if(m_blocks_sent.find(p) != NULL)
		m_blocks_sent.remove(p);
}

void RemoteClient::SetBlocksNotSent(core::map<v3s16, MapBlock*> &blocks)
{
	JMutexAutoLock sendinglock(m_blocks_sending_mutex);
	JMutexAutoLock sentlock(m_blocks_sent_mutex);

	m_nearest_unsent_d = 0;
	
	for(core::map<v3s16, MapBlock*>::Iterator
			i = blocks.getIterator();
			i.atEnd()==false; i++)
	{
		v3s16 p = i.getNode()->getKey();

		if(m_blocks_sending.find(p) != NULL)
			m_blocks_sending.remove(p);
		if(m_blocks_sent.find(p) != NULL)
			m_blocks_sent.remove(p);
	}
}

/*void RemoteClient::BlockEmerged()
{
	SharedPtr<JMutexAutoLock> lock(m_num_blocks_in_emerge_queue.getLock());
	assert(m_num_blocks_in_emerge_queue.m_value > 0);
	m_num_blocks_in_emerge_queue.m_value--;
}*/

/*void RemoteClient::RunSendingTimeouts(float dtime, float timeout)
{
	JMutexAutoLock sendinglock(m_blocks_sending_mutex);
	
	core::list<v3s16> remove_queue;
	for(core::map<v3s16, float>::Iterator
			i = m_blocks_sending.getIterator();
			i.atEnd()==false; i++)
	{
		v3s16 p = i.getNode()->getKey();
		float t = i.getNode()->getValue();
		t += dtime;
		i.getNode()->setValue(t);

		if(t > timeout)
		{
			remove_queue.push_back(p);
		}
	}
	for(core::list<v3s16>::Iterator
			i = remove_queue.begin();
			i != remove_queue.end(); i++)
	{
		m_blocks_sending.remove(*i);
	}
}*/

/*
	PlayerInfo
*/

PlayerInfo::PlayerInfo()
{
	name[0] = 0;
}

void PlayerInfo::PrintLine(std::ostream *s)
{
	(*s)<<id<<": \""<<name<<"\" ("
			<<position.X<<","<<position.Y
			<<","<<position.Z<<") ";
	address.print(s);
	(*s)<<" avg_rtt="<<avg_rtt;
	(*s)<<std::endl;
}

u32 PIChecksum(core::list<PlayerInfo> &l)
{
	core::list<PlayerInfo>::Iterator i;
	u32 checksum = 1;
	u32 a = 10;
	for(i=l.begin(); i!=l.end(); i++)
	{
		checksum += a * (i->id+1);
		checksum ^= 0x435aafcd;
		a *= 10;
	}
	return checksum;
}

/*
	Server
*/

Server::Server(
		std::string mapsavedir,
		HMParams hm_params,
		MapParams map_params
	):
	m_env(new ServerMap(mapsavedir, hm_params, map_params), dout_server),
	m_con(PROTOCOL_ID, 512, CONNECTION_TIMEOUT, this),
	m_thread(this),
	m_emergethread(this)
{
	m_env_mutex.Init();
	m_con_mutex.Init();
	m_step_dtime_mutex.Init();
	m_step_dtime = 0.0;
}

Server::~Server()
{
	// Stop threads
	stop();

	JMutexAutoLock clientslock(m_con_mutex);

	for(core::map<u16, RemoteClient*>::Iterator
		i = m_clients.getIterator();
		i.atEnd() == false; i++)
	{
		u16 peer_id = i.getNode()->getKey();

		// Delete player
		{
			JMutexAutoLock envlock(m_env_mutex);
			m_env.removePlayer(peer_id);
		}
		
		// Delete client
		delete i.getNode()->getValue();
	}
}

void Server::start(unsigned short port)
{
	// Stop thread if already running
	m_thread.stop();
	
	// Initialize connection
	m_con.setTimeoutMs(30);
	m_con.Serve(port);

	// Start thread
	m_thread.setRun(true);
	m_thread.Start();
	
	dout_server<<"Server started on port "<<port<<std::endl;
}

void Server::stop()
{
	// Stop threads (set run=false first so both start stopping)
	m_thread.setRun(false);
	m_emergethread.setRun(false);
	m_thread.stop();
	m_emergethread.stop();
	
	dout_server<<"Server threads stopped"<<std::endl;
}

void Server::step(float dtime)
{
	// Limit a bit
	if(dtime > 2.0)
		dtime = 2.0;
	{
		JMutexAutoLock lock(m_step_dtime_mutex);
		m_step_dtime += dtime;
	}
}

void Server::AsyncRunStep()
{
	float dtime;
	{
		JMutexAutoLock lock1(m_step_dtime_mutex);
		dtime = m_step_dtime;
		if(dtime < 0.001)
			return;
		m_step_dtime = 0.0;
	}
	
	//dstream<<"Server steps "<<dtime<<std::endl;
	
	//dstream<<"Server::AsyncRunStep(): dtime="<<dtime<<std::endl;
	{
		// Has to be locked for peerAdded/Removed
		JMutexAutoLock lock1(m_env_mutex);
		// Process connection's timeouts
		JMutexAutoLock lock2(m_con_mutex);
		m_con.RunTimeouts(dtime);
	}
	{
		// Step environment
		// This also runs Map's timers
		JMutexAutoLock lock(m_env_mutex);
		m_env.step(dtime);
	}
	
	/*
		Do background stuff
	*/
	
	// Periodically print some info
	{
		static float counter = 0.0;
		counter += dtime;
		if(counter >= 30.0)
		{
			counter = 0.0;

			JMutexAutoLock lock2(m_con_mutex);

			for(core::map<u16, RemoteClient*>::Iterator
				i = m_clients.getIterator();
				i.atEnd() == false; i++)
			{
				//u16 peer_id = i.getNode()->getKey();
				RemoteClient *client = i.getNode()->getValue();
				client->PrintInfo(std::cout);
			}
		}
	}

	// Run time- and client- related stuff
	// NOTE: If you intend to add something here, check that it
	// doesn't fit in RemoteClient::GetNextBlocks for example.
	/*{
		// Clients are behind connection lock
		JMutexAutoLock lock(m_con_mutex);

		for(core::map<u16, RemoteClient*>::Iterator
			i = m_clients.getIterator();
			i.atEnd() == false; i++)
		{
			RemoteClient *client = i.getNode()->getValue();
			//con::Peer *peer = m_con.GetPeer(client->peer_id);
			//client->RunSendingTimeouts(dtime, peer->resend_timeout);
		}
	}*/

	// Send blocks to clients
	SendBlocks(dtime);
	
	// Send object positions
	{
		static float counter = 0.0;
		counter += dtime;
		if(counter >= g_settings.getFloat("objectdata_interval"))
		{
			JMutexAutoLock lock1(m_env_mutex);
			JMutexAutoLock lock2(m_con_mutex);
			SendObjectData(counter);

			counter = 0.0;
		}
	}
	
	// Trigger emergethread (it gets somehow gets to a
	// non-triggered but bysy state sometimes)
	{
		static float counter = 0.0;
		counter += dtime;
		if(counter >= 2.0)
		{
			counter = 0.0;
			
			m_emergethread.trigger();
		}
	}

	// Save map
	{
		static float counter = 0.0;
		counter += dtime;
		if(counter >= SERVER_MAP_SAVE_INTERVAL)
		{
			counter = 0.0;

			JMutexAutoLock lock(m_env_mutex);
			// Save only changed parts
			m_env.getMap().save(true);
		}
	}
}

void Server::Receive()
{
	u32 data_maxsize = 10000;
	Buffer<u8> data(data_maxsize);
	u16 peer_id;
	u32 datasize;
	try{
		{
			JMutexAutoLock lock(m_con_mutex);
			datasize = m_con.Receive(peer_id, *data, data_maxsize);
		}
		ProcessData(*data, datasize, peer_id);
	}
	catch(con::InvalidIncomingDataException &e)
	{
		derr_server<<"Server::Receive(): "
				"InvalidIncomingDataException: what()="
				<<e.what()<<std::endl;
	}
	catch(con::PeerNotFoundException &e)
	{
		//NOTE: This is not needed anymore
		
		// The peer has been disconnected.
		// Find the associated player and remove it.

		/*JMutexAutoLock envlock(m_env_mutex);

		dout_server<<"ServerThread: peer_id="<<peer_id
				<<" has apparently closed connection. "
				<<"Removing player."<<std::endl;

		m_env.removePlayer(peer_id);*/
	}
}

void Server::ProcessData(u8 *data, u32 datasize, u16 peer_id)
{
	// Environment is locked first.
	JMutexAutoLock envlock(m_env_mutex);
	JMutexAutoLock conlock(m_con_mutex);
	
	con::Peer *peer;
	try{
		peer = m_con.GetPeer(peer_id);
	}
	catch(con::PeerNotFoundException &e)
	{
		derr_server<<DTIME<<"Server::ProcessData(): Cancelling: peer "
				<<peer_id<<" not found"<<std::endl;
		return;
	}
	
	//u8 peer_ser_ver = peer->serialization_version;
	u8 peer_ser_ver = getClient(peer->id)->serialization_version;

	try
	{

	if(datasize < 2)
		return;

	ToServerCommand command = (ToServerCommand)readU16(&data[0]);
	
	if(command == TOSERVER_INIT)
	{
		// [0] u16 TOSERVER_INIT
		// [2] u8 SER_FMT_VER_HIGHEST
		// [3] u8[20] player_name

		if(datasize < 3)
			return;

		derr_server<<DTIME<<"Server: Got TOSERVER_INIT from "
				<<peer->id<<std::endl;

		// First byte after command is maximum supported
		// serialization version
		u8 client_max = data[2];
		u8 our_max = SER_FMT_VER_HIGHEST;
		// Use the highest version supported by both
		u8 deployed = core::min_(client_max, our_max);
		// If it's lower than the lowest supported, give up.
		if(deployed < SER_FMT_VER_LOWEST)
			deployed = SER_FMT_VER_INVALID;

		//peer->serialization_version = deployed;
		getClient(peer->id)->pending_serialization_version = deployed;

		if(deployed == SER_FMT_VER_INVALID)
		{
			derr_server<<DTIME<<"Server: Cannot negotiate "
					"serialization version with peer "
					<<peer_id<<std::endl;
			return;
		}

		/*
			Set up player
		*/

		Player *player = m_env.getPlayer(peer_id);

		// Check if player doesn't exist
		if(player == NULL)
			throw con::InvalidIncomingDataException
				("Server::ProcessData(): INIT: Player doesn't exist");

		// update name if it was supplied
		if(datasize >= 20+3)
		{
			data[20+3-1] = 0;
			player->updateName((const char*)&data[3]);
		}

		// Now answer with a TOCLIENT_INIT
		
		SharedBuffer<u8> reply(2+1+6);
		writeU16(&reply[0], TOCLIENT_INIT);
		writeU8(&reply[2], deployed);
		writeV3S16(&reply[3], floatToInt(player->getPosition()+v3f(0,BS/2,0)));
		// Send as reliable
		m_con.Send(peer_id, 0, reply, true);

		return;
	}
	if(command == TOSERVER_INIT2)
	{
		derr_server<<DTIME<<"Server: Got TOSERVER_INIT2 from "
				<<peer->id<<std::endl;


		getClient(peer->id)->serialization_version
				= getClient(peer->id)->pending_serialization_version;

		/*
			Send some initialization data
		*/
		
		// Send player info to all players
		SendPlayerInfos();

		// Send inventory to player
		SendInventory(peer->id);

		return;
	}

	if(peer_ser_ver == SER_FMT_VER_INVALID)
	{
		derr_server<<DTIME<<"Server::ProcessData(): Cancelling: Peer"
				" serialization format invalid or not initialized."
				" Skipping incoming command="<<command<<std::endl;
		return;
	}
	
	Player *player = m_env.getPlayer(peer_id);

	if(player == NULL){
		derr_server<<"Server::ProcessData(): Cancelling: "
				"No player for peer_id="<<peer_id
				<<std::endl;
		return;
	}
	if(command == TOSERVER_PLAYERPOS)
	{
		if(datasize < 2+12+12+4+4)
			return;
	
		u32 start = 0;
		v3s32 ps = readV3S32(&data[start+2]);
		v3s32 ss = readV3S32(&data[start+2+12]);
		f32 pitch = (f32)readS32(&data[2+12+12]) / 100.0;
		f32 yaw = (f32)readS32(&data[2+12+12+4]) / 100.0;
		v3f position((f32)ps.X/100., (f32)ps.Y/100., (f32)ps.Z/100.);
		v3f speed((f32)ss.X/100., (f32)ss.Y/100., (f32)ss.Z/100.);
		pitch = wrapDegrees(pitch);
		yaw = wrapDegrees(yaw);
		player->setPosition(position);
		player->setSpeed(speed);
		player->setPitch(pitch);
		player->setYaw(yaw);
		
		/*dout_server<<"Server::ProcessData(): Moved player "<<peer_id<<" to "
				<<"("<<position.X<<","<<position.Y<<","<<position.Z<<")"
				<<" pitch="<<pitch<<" yaw="<<yaw<<std::endl;*/
	}
	else if(command == TOSERVER_GOTBLOCKS)
	{
		if(datasize < 2+1)
			return;
		
		/*
			[0] u16 command
			[2] u8 count
			[3] v3s16 pos_0
			[3+6] v3s16 pos_1
			...
		*/

		u16 count = data[2];
		for(u16 i=0; i<count; i++)
		{
			if((s16)datasize < 2+1+(i+1)*6)
				throw con::InvalidIncomingDataException
					("GOTBLOCKS length is too short");
			v3s16 p = readV3S16(&data[2+1+i*6]);
			/*dstream<<"Server: GOTBLOCKS ("
					<<p.X<<","<<p.Y<<","<<p.Z<<")"<<std::endl;*/
			RemoteClient *client = getClient(peer_id);
			client->GotBlock(p);
		}
	}
	else if(command == TOSERVER_DELETEDBLOCKS)
	{
		if(datasize < 2+1)
			return;
		
		/*
			[0] u16 command
			[2] u8 count
			[3] v3s16 pos_0
			[3+6] v3s16 pos_1
			...
		*/

		u16 count = data[2];
		for(u16 i=0; i<count; i++)
		{
			if((s16)datasize < 2+1+(i+1)*6)
				throw con::InvalidIncomingDataException
					("DELETEDBLOCKS length is too short");
			v3s16 p = readV3S16(&data[2+1+i*6]);
			/*dstream<<"Server: DELETEDBLOCKS ("
					<<p.X<<","<<p.Y<<","<<p.Z<<")"<<std::endl;*/
			RemoteClient *client = getClient(peer_id);
			client->SetBlockNotSent(p);
		}
	}
	else if(command == TOSERVER_CLICK_OBJECT)
	{
		if(datasize < 13)
			return;

		/*
			[0] u16 command
			[2] u8 button (0=left, 1=right)
			[3] v3s16 block
			[9] s16 id
			[11] u16 item
		*/
		u8 button = readU8(&data[2]);
		v3s16 p;
		p.X = readS16(&data[3]);
		p.Y = readS16(&data[5]);
		p.Z = readS16(&data[7]);
		s16 id = readS16(&data[9]);
		//u16 item_i = readU16(&data[11]);

		MapBlock *block = NULL;
		try
		{
			block = m_env.getMap().getBlockNoCreate(p);
		}
		catch(InvalidPositionException &e)
		{
			derr_server<<"PICK_OBJECT block not found"<<std::endl;
			return;
		}

		MapBlockObject *obj = block->getObject(id);

		if(obj == NULL)
		{
			derr_server<<"PICK_OBJECT object not found"<<std::endl;
			return;
		}

		//TODO: Check that object is reasonably close
		
		// Left click
		if(button == 0)
		{
			if(g_settings.getBool("creative_mode") == false)
			{
			
				// Skip if inventory has no free space
				if(player->inventory.getUsedSlots() == player->inventory.getSize())
				{
					dout_server<<"Player inventory has no free space"<<std::endl;
					return;
				}
			
				// Add to inventory and send inventory
				InventoryItem *item = new MapBlockObjectItem
						(obj->getInventoryString());
				player->inventory.addItem(item);
				SendInventory(player->peer_id);
			}

			// Remove from block
			block->removeObject(id);
		}
	}
	else if(command == TOSERVER_CLICK_GROUND)
	{
		if(datasize < 17)
			return;
		/*
			length: 17
			[0] u16 command
			[2] u8 button (0=left, 1=right)
			[3] v3s16 nodepos_undersurface
			[9] v3s16 nodepos_abovesurface
			[15] u16 item
		*/
		u8 button = readU8(&data[2]);
		v3s16 p_under;
		p_under.X = readS16(&data[3]);
		p_under.Y = readS16(&data[5]);
		p_under.Z = readS16(&data[7]);
		v3s16 p_over;
		p_over.X = readS16(&data[9]);
		p_over.Y = readS16(&data[11]);
		p_over.Z = readS16(&data[13]);
		u16 item_i = readU16(&data[15]);

		//TODO: Check that target is reasonably close
		
		/*
			Left button digs ground
		*/
		if(button == 0)
		{

			core::map<v3s16, MapBlock*> modified_blocks;

			u8 material;

			try
			{
				// Get material at position
				material = m_env.getMap().getNode(p_under).d;
				// If it's air, do nothing
				if(material == MATERIAL_AIR)
				{
					return;
				}
			}
			catch(InvalidPositionException &e)
			{
				derr_server<<"Server: Ignoring REMOVENODE: Node not found"
						<<std::endl;
				return;
			}
			
			// Reset build time counter
			getClient(peer->id)->m_time_from_building.set(0.0);
			
			// Create packet
			u32 replysize = 8;
			SharedBuffer<u8> reply(replysize);
			writeU16(&reply[0], TOCLIENT_REMOVENODE);
			writeS16(&reply[2], p_under.X);
			writeS16(&reply[4], p_under.Y);
			writeS16(&reply[6], p_under.Z);
			// Send as reliable
			m_con.SendToAll(0, reply, true);
			
			if(g_settings.getBool("creative_mode") == false)
			{
				// Add to inventory and send inventory
				InventoryItem *item = new MaterialItem(material, 1);
				player->inventory.addItem(item);
				SendInventory(player->peer_id);
			}

			/*
				Remove the node
				(this takes some time so it is done after the quick stuff)
			*/
			m_env.getMap().removeNodeAndUpdate(p_under, modified_blocks);

		} // button == 0
		/*
			Right button places blocks and stuff
		*/
		else if(button == 1)
		{

			// Get item
			InventoryItem *item = player->inventory.getItem(item_i);
			
			// If there is no item, it is not possible to add it anywhere
			if(item == NULL)
				return;
			
			/*
				Handle material items
			*/
			if(std::string("MaterialItem") == item->getName())
			{
				MaterialItem *mitem = (MaterialItem*)item;
				
				MapNode n;
				n.d = mitem->getMaterial();

				try{
					// Don't add a node if there isn't air
					MapNode n2 = m_env.getMap().getNode(p_over);
					if(n2.d != MATERIAL_AIR)
						return;
				}
				catch(InvalidPositionException &e)
				{
					derr_server<<"Server: Ignoring ADDNODE: Node not found"
							<<std::endl;
					return;
				}

				// Reset build time counter
				getClient(peer->id)->m_time_from_building.set(0.0);
				
				if(g_settings.getBool("creative_mode") == false)
				{
					// Remove from inventory and send inventory
					if(mitem->getCount() == 1)
						player->inventory.deleteItem(item_i);
					else
						mitem->remove(1);
					// Send inventory
					SendInventory(peer_id);
				}
				
				// Create packet
				u32 replysize = 8 + MapNode::serializedLength(peer_ser_ver);
				SharedBuffer<u8> reply(replysize);
				writeU16(&reply[0], TOCLIENT_ADDNODE);
				writeS16(&reply[2], p_over.X);
				writeS16(&reply[4], p_over.Y);
				writeS16(&reply[6], p_over.Z);
				n.serialize(&reply[8], peer_ser_ver);
				// Send as reliable
				m_con.SendToAll(0, reply, true);
				
				/*
					Add node.

					This takes some time so it is done after the quick stuff
				*/
				core::map<v3s16, MapBlock*> modified_blocks;
				m_env.getMap().addNodeAndUpdate(p_over, n, modified_blocks);
			}
			/*
				Handle block object items
			*/
			else if(std::string("MBOItem") == item->getName())
			{
				MapBlockObjectItem *oitem = (MapBlockObjectItem*)item;

				/*dout_server<<"Trying to place a MapBlockObjectItem: "
						"inventorystring=\""
						<<oitem->getInventoryString()
						<<"\""<<std::endl;*/

				v3s16 blockpos = getNodeBlockPos(p_over);

				MapBlock *block = NULL;
				try
				{
					block = m_env.getMap().getBlockNoCreate(blockpos);
				}
				catch(InvalidPositionException &e)
				{
					derr_server<<"Error while placing object: "
							"block not found"<<std::endl;
					return;
				}

				v3s16 block_pos_i_on_map = block->getPosRelative();
				v3f block_pos_f_on_map = intToFloat(block_pos_i_on_map);

				v3f pos = intToFloat(p_over);
				pos -= block_pos_f_on_map;
				
				/*dout_server<<"pos="
						<<"("<<pos.X<<","<<pos.Y<<","<<pos.Z<<")"
						<<std::endl;*/


				MapBlockObject *obj = oitem->createObject
						(pos, player->getYaw(), player->getPitch());

				if(obj == NULL)
					derr_server<<"WARNING: oitem created NULL object"
							<<std::endl;

				block->addObject(obj);

				//dout_server<<"Placed object"<<std::endl;

				if(g_settings.getBool("creative_mode") == false)
				{
					// Remove from inventory and send inventory
					player->inventory.deleteItem(item_i);
					// Send inventory
					SendInventory(peer_id);
				}
			}

		} // button == 1
		/*
			Catch invalid buttons
		*/
		else
		{
			derr_server<<"WARNING: Server: Invalid button "
					<<button<<std::endl;
		}
	}
	else if(command == TOSERVER_RELEASE)
	{
		if(datasize < 3)
			return;
		/*
			length: 3
			[0] u16 command
			[2] u8 button
		*/
		//TODO
	}
	else if(command == TOSERVER_SIGNTEXT)
	{
		/*
			u16 command
			v3s16 blockpos
			s16 id
			u16 textlen
			textdata
		*/
		std::string datastring((char*)&data[2], datasize-2);
		std::istringstream is(datastring, std::ios_base::binary);
		u8 buf[6];
		// Read stuff
		is.read((char*)buf, 6);
		v3s16 blockpos = readV3S16(buf);
		is.read((char*)buf, 2);
		s16 id = readS16(buf);
		is.read((char*)buf, 2);
		u16 textlen = readU16(buf);
		std::string text;
		for(u16 i=0; i<textlen; i++)
		{
			is.read((char*)buf, 1);
			text += (char)buf[0];
		}

		MapBlock *block = NULL;
		try
		{
			block = m_env.getMap().getBlockNoCreate(blockpos);
		}
		catch(InvalidPositionException &e)
		{
			derr_server<<"Error while setting sign text: "
					"block not found"<<std::endl;
			return;
		}

		MapBlockObject *obj = block->getObject(id);
		if(obj == NULL)
		{
			derr_server<<"Error while setting sign text: "
					"object not found"<<std::endl;
			return;
		}
		
		if(obj->getTypeId() != MAPBLOCKOBJECT_TYPE_SIGN)
		{
			derr_server<<"Error while setting sign text: "
					"object is not a sign"<<std::endl;
			return;
		}

		((SignObject*)obj)->setText(text);

		obj->getBlock()->setChangedFlag();
	}
	else
	{
		derr_server<<"WARNING: Server::ProcessData(): Ignoring "
				"unknown command "<<command<<std::endl;
	}
	
	} //try
	catch(SendFailedException &e)
	{
		derr_server<<"Server::ProcessData(): SendFailedException: "
				<<"what="<<e.what()
				<<std::endl;
	}
}

/*void Server::Send(u16 peer_id, u16 channelnum,
		SharedBuffer<u8> data, bool reliable)
{
	JMutexAutoLock lock(m_con_mutex);
	m_con.Send(peer_id, channelnum, data, reliable);
}*/

void Server::SendBlockNoLock(u16 peer_id, MapBlock *block, u8 ver)
{
	/*
		Create a packet with the block in the right format
	*/
	
	std::ostringstream os(std::ios_base::binary);
	block->serialize(os, ver);
	std::string s = os.str();
	SharedBuffer<u8> blockdata((u8*)s.c_str(), s.size());

	u32 replysize = 8 + blockdata.getSize();
	SharedBuffer<u8> reply(replysize);
	v3s16 p = block->getPos();
	writeU16(&reply[0], TOCLIENT_BLOCKDATA);
	writeS16(&reply[2], p.X);
	writeS16(&reply[4], p.Y);
	writeS16(&reply[6], p.Z);
	memcpy(&reply[8], *blockdata, blockdata.getSize());
	
	/*
		Send packet
	*/
	m_con.Send(peer_id, 1, reply, true);
}

/*void Server::SendBlock(u16 peer_id, MapBlock *block, u8 ver)
{
	JMutexAutoLock conlock(m_con_mutex);
	
	SendBlockNoLock(peer_id, block, ver);
}*/

#if 0
void Server::SendSectorMeta(u16 peer_id, core::list<v2s16> ps, u8 ver)
{
	DSTACK(__FUNCTION_NAME);
	dstream<<"Server sending sector meta of "
			<<ps.getSize()<<" sectors"<<std::endl;

	core::list<v2s16>::Iterator i = ps.begin();
	core::list<v2s16> sendlist;
	for(;;)
	{
		if(sendlist.size() == 255 || i == ps.end())
		{
			if(sendlist.size() == 0)
				break;
			/*
				[0] u16 command
				[2] u8 sector count
				[3...] v2s16 pos + sector metadata
			*/
			std::ostringstream os(std::ios_base::binary);
			u8 buf[4];

			writeU16(buf, TOCLIENT_SECTORMETA);
			os.write((char*)buf, 2);

			writeU8(buf, sendlist.size());
			os.write((char*)buf, 1);

			for(core::list<v2s16>::Iterator
					j = sendlist.begin();
					j != sendlist.end(); j++)
			{
				// Write position
				writeV2S16(buf, *j);
				os.write((char*)buf, 4);
				
				/*
					Write ClientMapSector metadata
				*/

				/*
					[0] u8 serialization version
					[1] s16 corners[0]
					[3] s16 corners[1]
					[5] s16 corners[2]
					[7] s16 corners[3]
					size = 9
					
					In which corners are in these positions
					v2s16(0,0),
					v2s16(1,0),
					v2s16(1,1),
					v2s16(0,1),
				*/

				// Write version
				writeU8(buf, ver);
				os.write((char*)buf, 1);

				// Write corners
				// TODO: Get real values
				s16 corners[4];
				((ServerMap&)m_env.getMap()).getSectorCorners(*j, corners);

				writeS16(buf, corners[0]);
				os.write((char*)buf, 2);
				writeS16(buf, corners[1]);
				os.write((char*)buf, 2);
				writeS16(buf, corners[2]);
				os.write((char*)buf, 2);
				writeS16(buf, corners[3]);
				os.write((char*)buf, 2);
			}

			SharedBuffer<u8> data((u8*)os.str().c_str(), os.str().size());

			/*dstream<<"Server::SendSectorMeta(): sending packet"
					" with "<<sendlist.size()<<" sectors"<<std::endl;*/

			m_con.Send(peer_id, 1, data, true);

			if(i == ps.end())
				break;

			sendlist.clear();
		}

		sendlist.push_back(*i);
		i++;
	}
}
#endif

core::list<PlayerInfo> Server::getPlayerInfo()
{
	JMutexAutoLock envlock(m_env_mutex);
	JMutexAutoLock conlock(m_con_mutex);
	
	core::list<PlayerInfo> list;

	core::list<Player*> players = m_env.getPlayers();
	
	core::list<Player*>::Iterator i;
	for(i = players.begin();
			i != players.end(); i++)
	{
		PlayerInfo info;

		Player *player = *i;
		try{
			con::Peer *peer = m_con.GetPeer(player->peer_id);
			info.id = peer->id;
			info.address = peer->address;
			info.avg_rtt = peer->avg_rtt;
		}
		catch(con::PeerNotFoundException &e)
		{
			// Outdated peer info
			info.id = 0;
			info.address = Address(0,0,0,0,0);
			info.avg_rtt = 0.0;
		}

		snprintf(info.name, PLAYERNAME_SIZE, "%s", player->getName());
		info.position = player->getPosition();

		list.push_back(info);
	}

	return list;
}

void Server::peerAdded(con::Peer *peer)
{
	dout_server<<"Server::peerAdded(): peer->id="
			<<peer->id<<std::endl;
	
	// Connection is already locked when this is called.
	//JMutexAutoLock lock(m_con_mutex);
	
	// Error check
	core::map<u16, RemoteClient*>::Node *n;
	n = m_clients.find(peer->id);
	// The client shouldn't already exist
	assert(n == NULL);

	// Create client
	RemoteClient *client = new RemoteClient();
	client->peer_id = peer->id;
	m_clients.insert(client->peer_id, client);

	// Create player
	{
		// Already locked when called
		//JMutexAutoLock envlock(m_env_mutex);
		
		Player *player = m_env.getPlayer(peer->id);
		
		// The player shouldn't already exist
		assert(player == NULL);

		player = new RemotePlayer();
		player->peer_id = peer->id;

		/*
			Set player position
		*/

		// Get zero sector (it could have been unloaded to disk)
		m_env.getMap().emergeSector(v2s16(0,0));
		// Get ground height at origin
		f32 groundheight = m_env.getMap().getGroundHeight(v2s16(0,0), true);
		// The zero sector should have been generated
		assert(groundheight > GROUNDHEIGHT_VALID_MINVALUE);
		// Don't go underwater
		if(groundheight < WATER_LEVEL)
			groundheight = WATER_LEVEL;

		player->setPosition(intToFloat(v3s16(
				0,
				groundheight + 1,
				0
		)));

		/*
			Add player to environment
		*/

		m_env.addPlayer(player);

		/*
			Add stuff to inventory
		*/
		
		if(g_settings.getBool("creative_mode"))
		{
			// Give all materials
			assert(USEFUL_MATERIAL_COUNT <= PLAYER_INVENTORY_SIZE);
			for(u16 i=0; i<USEFUL_MATERIAL_COUNT; i++)
			{
				InventoryItem *item = new MaterialItem(i, 1);
				player->inventory.addItem(item);
			}
			// Sign
			{
				InventoryItem *item = new MapBlockObjectItem("Sign Example text");
				bool r = player->inventory.addItem(item);
				assert(r == true);
			}
			/*// Rat
			{
				InventoryItem *item = new MapBlockObjectItem("Rat");
				bool r = player->inventory.addItem(item);
				assert(r == true);
			}*/
		}
		else
		{
			// Give some lights
			{
				InventoryItem *item = new MaterialItem(3, 999);
				bool r = player->inventory.addItem(item);
				assert(r == true);
			}
			// and some signs
			for(u16 i=0; i<4; i++)
			{
				InventoryItem *item = new MapBlockObjectItem("Sign Example text");
				bool r = player->inventory.addItem(item);
				assert(r == true);
			}
			/*// and some rats
			for(u16 i=0; i<4; i++)
			{
				InventoryItem *item = new MapBlockObjectItem("Rat");
				bool r = player->inventory.addItem(item);
				assert(r == true);
			}*/
		}
	}
}

void Server::deletingPeer(con::Peer *peer, bool timeout)
{
	dout_server<<"Server::deletingPeer(): peer->id="
			<<peer->id<<", timeout="<<timeout<<std::endl;
	
	// Connection is already locked when this is called.
	//JMutexAutoLock lock(m_con_mutex);

	// Error check
	core::map<u16, RemoteClient*>::Node *n;
	n = m_clients.find(peer->id);
	// The client should exist
	assert(n != NULL);
	
	// Delete player
	{
		// Already locked when called
		//JMutexAutoLock envlock(m_env_mutex);
		m_env.removePlayer(peer->id);
	}
	
	// Delete client
	delete m_clients[peer->id];
	m_clients.remove(peer->id);

	// Send player info to all clients
	SendPlayerInfos();
}

void Server::SendObjectData(float dtime)
{
	core::map<v3s16, bool> stepped_blocks;
	
	for(core::map<u16, RemoteClient*>::Iterator
		i = m_clients.getIterator();
		i.atEnd() == false; i++)
	{
		u16 peer_id = i.getNode()->getKey();
		RemoteClient *client = i.getNode()->getValue();
		assert(client->peer_id == peer_id);
		
		if(client->serialization_version == SER_FMT_VER_INVALID)
			continue;
		
		client->SendObjectData(this, dtime, stepped_blocks);
	}
}

void Server::SendPlayerInfos()
{
	//JMutexAutoLock envlock(m_env_mutex);
	
	core::list<Player*> players = m_env.getPlayers();
	
	u32 player_count = players.getSize();
	u32 datasize = 2+(2+PLAYERNAME_SIZE)*player_count;

	SharedBuffer<u8> data(datasize);
	writeU16(&data[0], TOCLIENT_PLAYERINFO);
	
	u32 start = 2;
	core::list<Player*>::Iterator i;
	for(i = players.begin();
			i != players.end(); i++)
	{
		Player *player = *i;

		/*dstream<<"Server sending player info for player with "
				"peer_id="<<player->peer_id<<std::endl;*/
		
		writeU16(&data[start], player->peer_id);
		snprintf((char*)&data[start+2], PLAYERNAME_SIZE, "%s", player->getName());
		start += 2+PLAYERNAME_SIZE;
	}

	//JMutexAutoLock conlock(m_con_mutex);

	// Send as reliable
	m_con.SendToAll(0, data, true);
}

void Server::SendInventory(u16 peer_id)
{
	//JMutexAutoLock envlock(m_env_mutex);
	
	Player* player = m_env.getPlayer(peer_id);

	std::ostringstream os;
	//os.imbue(std::locale("C"));

	player->inventory.serialize(os);

	std::string s = os.str();
	
	SharedBuffer<u8> data(s.size()+2);
	writeU16(&data[0], TOCLIENT_INVENTORY);
	memcpy(&data[2], s.c_str(), s.size());
	
	//JMutexAutoLock conlock(m_con_mutex);

	// Send as reliable
	m_con.Send(peer_id, 0, data, true);
}

void Server::SendBlocks(float dtime)
{
	JMutexAutoLock envlock(m_env_mutex);

	core::array<PrioritySortedBlockTransfer> queue;

	s32 total_sending = 0;

	for(core::map<u16, RemoteClient*>::Iterator
		i = m_clients.getIterator();
		i.atEnd() == false; i++)
	{
		RemoteClient *client = i.getNode()->getValue();
		assert(client->peer_id == i.getNode()->getKey());

		total_sending += client->SendingCount();
		
		if(client->serialization_version == SER_FMT_VER_INVALID)
			continue;
		
		client->GetNextBlocks(this, dtime, queue);
	}

	// Sort.
	// Lowest priority number comes first.
	// Lowest is most important.
	queue.sort();

	JMutexAutoLock conlock(m_con_mutex);

	for(u32 i=0; i<queue.size(); i++)
	{
		//TODO: Calculate limit dynamically
		if(total_sending >= g_settings.getS32
				("max_simultaneous_block_sends_server_total"))
			break;
		
		PrioritySortedBlockTransfer q = queue[i];

		MapBlock *block = NULL;
		try
		{
			block = m_env.getMap().getBlockNoCreate(q.pos);
		}
		catch(InvalidPositionException &e)
		{
			continue;
		}

		RemoteClient *client = getClient(q.peer_id);

		SendBlockNoLock(q.peer_id, block, client->serialization_version);

		client->SentBlock(q.pos);

		total_sending++;
	}
}


RemoteClient* Server::getClient(u16 peer_id)
{
	//JMutexAutoLock lock(m_con_mutex);
	core::map<u16, RemoteClient*>::Node *n;
	n = m_clients.find(peer_id);
	// A client should exist for all peers
	assert(n != NULL);
	return n->getValue();
}


