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

#ifndef SERVER_HEADER
#define SERVER_HEADER

#include "connection.h"
#include "environment.h"
#include "common_irrlicht.h"
#include <string>

#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)

struct QueuedBlockEmerge
{
	v3s16 pos;
	// key = peer_id, value = flags
	core::map<u16, u8> peer_ids;
};

/*
	This is a thread-safe class.
*/
class BlockEmergeQueue
{
public:
	BlockEmergeQueue()
	{
		m_mutex.Init();
	}

	~BlockEmergeQueue()
	{
		JMutexAutoLock lock(m_mutex);

		core::list<QueuedBlockEmerge*>::Iterator i;
		for(i=m_queue.begin(); i!=m_queue.end(); i++)
		{
			QueuedBlockEmerge *q = *i;
			delete q;
		}
	}
	
	/*
		peer_id=0 adds with nobody to send to
	*/
	void addBlock(u16 peer_id, v3s16 pos, u8 flags)
	{
		DSTACK(__FUNCTION_NAME);
	
		JMutexAutoLock lock(m_mutex);

		if(peer_id != 0)
		{
			/*
				Find if block is already in queue.
				If it is, update the peer to it and quit.
			*/
			core::list<QueuedBlockEmerge*>::Iterator i;
			for(i=m_queue.begin(); i!=m_queue.end(); i++)
			{
				QueuedBlockEmerge *q = *i;
				if(q->pos == pos)
				{
					q->peer_ids[peer_id] = flags;
					return;
				}
			}
		}
		
		/*
			Add the block
		*/
		QueuedBlockEmerge *q = new QueuedBlockEmerge;
		q->pos = pos;
		if(peer_id != 0)
			q->peer_ids[peer_id] = flags;
		m_queue.push_back(q);
	}

	// Returned pointer must be deleted
	// Returns NULL if queue is empty
	QueuedBlockEmerge * pop()
	{
		JMutexAutoLock lock(m_mutex);

		core::list<QueuedBlockEmerge*>::Iterator i = m_queue.begin();
		if(i == m_queue.end())
			return NULL;
		QueuedBlockEmerge *q = *i;
		m_queue.erase(i);
		return q;
	}

	u32 size()
	{
		JMutexAutoLock lock(m_mutex);
		return m_queue.size();
	}
	
	u32 peerItemCount(u16 peer_id)
	{
		JMutexAutoLock lock(m_mutex);

		u32 count = 0;

		core::list<QueuedBlockEmerge*>::Iterator i;
		for(i=m_queue.begin(); i!=m_queue.end(); i++)
		{
			QueuedBlockEmerge *q = *i;
			if(q->peer_ids.find(peer_id) != NULL)
				count++;
		}

		return count;
	}

private:
	core::list<QueuedBlockEmerge*> m_queue;
	JMutex m_mutex;
};

class SimpleThread : public JThread
{
	bool run;
	JMutex run_mutex;

public:

	SimpleThread():
		JThread(),
		run(true)
	{
		run_mutex.Init();
	}

	virtual ~SimpleThread()
	{}

	virtual void * Thread() = 0;

	bool getRun()
	{
		JMutexAutoLock lock(run_mutex);
		return run;
	}
	void setRun(bool a_run)
	{
		JMutexAutoLock lock(run_mutex);
		run = a_run;
	}

	void stop()
	{
		setRun(false);
		while(IsRunning())
			sleep_ms(100);
	}
};

class Server;

class ServerThread : public SimpleThread
{
	Server *m_server;

public:

	ServerThread(Server *server):
		SimpleThread(),
		m_server(server)
	{
	}

	void * Thread();
};

class EmergeThread : public SimpleThread
{
	Server *m_server;

public:

	EmergeThread(Server *server):
		SimpleThread(),
		m_server(server)
	{
	}

	void * Thread();

	void trigger()
	{
		setRun(true);
		if(IsRunning() == false)
		{
			Start();
		}
	}
};

struct PlayerInfo
{
	u16 id;
	char name[PLAYERNAME_SIZE];
	v3f position;
	Address address;
	float avg_rtt;

	PlayerInfo();
	void PrintLine(std::ostream *s);
};

u32 PIChecksum(core::list<PlayerInfo> &l);

/*
	Used for queueing and sorting block transfers in containers
	
	Lower priority number means higher priority.
*/
struct PrioritySortedBlockTransfer
{
	PrioritySortedBlockTransfer(float a_priority, v3s16 a_pos, u16 a_peer_id)
	{
		priority = a_priority;
		pos = a_pos;
		peer_id = a_peer_id;
	}
	bool operator < (PrioritySortedBlockTransfer &other)
	{
		return priority < other.priority;
	}
	float priority;
	v3s16 pos;
	u16 peer_id;
};

class RemoteClient
{
public:
	// peer_id=0 means this client has no associated peer
	// NOTE: If client is made allowed to exist while peer doesn't,
	//       this has to be set to 0 when there is no peer.
	//       Also, the client must be moved to some other container.
	u16 peer_id;
	// The serialization version to use with the client
	u8 serialization_version;
	// Version is stored in here after INIT before INIT2
	u8 pending_serialization_version;

	RemoteClient():
		m_time_from_building(0.0)
		//m_num_blocks_in_emerge_queue(0)
	{
		peer_id = 0;
		serialization_version = SER_FMT_VER_INVALID;
		pending_serialization_version = SER_FMT_VER_INVALID;
		m_nearest_unsent_d = 0;

		m_blocks_sent_mutex.Init();
		m_blocks_sending_mutex.Init();
	}
	~RemoteClient()
	{
	}
	
	/*
		Finds block that should be sent next to the client.
		Environment should be locked when this is called.
		dtime is used for resetting send radius at slow interval
	*/
	void GetNextBlocks(Server *server, float dtime,
			core::array<PrioritySortedBlockTransfer> &dest);

	// Connection and environment should be locked when this is called
	// steps() objects of blocks not found in active_blocks, then
	// adds those blocks to active_blocks
	void SendObjectData(
			Server *server,
			float dtime,
			core::map<v3s16, bool> &stepped_blocks
		);

	void GotBlock(v3s16 p);

	void SentBlock(v3s16 p);

	void SetBlockNotSent(v3s16 p);
	void SetBlocksNotSent(core::map<v3s16, MapBlock*> &blocks);

	//void BlockEmerged();

	/*bool IsSendingBlock(v3s16 p)
	{
		JMutexAutoLock lock(m_blocks_sending_mutex);
		return (m_blocks_sending.find(p) != NULL);
	}*/

	s32 SendingCount()
	{
		JMutexAutoLock lock(m_blocks_sending_mutex);
		return m_blocks_sending.size();
	}
	
	// Increments timeouts and removes timed-out blocks from list
	// NOTE: This doesn't fix the server-not-sending-block bug
	//       because it is related to emerging, not sending.
	//void RunSendingTimeouts(float dtime, float timeout);

	void PrintInfo(std::ostream &o)
	{
		JMutexAutoLock l2(m_blocks_sent_mutex);
		JMutexAutoLock l3(m_blocks_sending_mutex);
		o<<"RemoteClient "<<peer_id<<": "
				/*<<"m_num_blocks_in_emerge_queue="
				<<m_num_blocks_in_emerge_queue.get()*/
				<<", m_blocks_sent.size()="<<m_blocks_sent.size()
				<<", m_blocks_sending.size()="<<m_blocks_sending.size()
				<<", m_nearest_unsent_d="<<m_nearest_unsent_d
				<<std::endl;
	}

	// Time from last placing or removing blocks
	MutexedVariable<float> m_time_from_building;
	
private:
	/*
		All members that are accessed by many threads should
		obviously be behind a mutex. The threads include:
		- main thread (calls step())
		- server thread (calls AsyncRunStep() and Receive())
		- emerge thread 
	*/
	
	//TODO: core::map<v3s16, MapBlock*> m_active_blocks
	//NOTE: Not here, it should be server-wide!

	// Number of blocks in the emerge queue that have this client as
	// a receiver. Used for throttling network usage.
	//MutexedVariable<s16> m_num_blocks_in_emerge_queue;

	/*
		Blocks that have been sent to client.
		- These don't have to be sent again.
		- A block is cleared from here when client says it has
		  deleted it from it's memory
		
		Key is position, value is dummy.
		No MapBlock* is stored here because the blocks can get deleted.
	*/
	core::map<v3s16, bool> m_blocks_sent;
	s16 m_nearest_unsent_d;
	v3s16 m_last_center;
	JMutex m_blocks_sent_mutex;
	/*
		Blocks that are currently on the line.
		This is used for throttling the sending of blocks.
		- The size of this list is limited to some value
		Block is added when it is sent with BLOCKDATA.
		Block is removed when GOTBLOCKS is received.
		Value is time from sending. (not used at the moment)
	*/
	core::map<v3s16, float> m_blocks_sending;
	JMutex m_blocks_sending_mutex;
};

/*struct ServerSettings
{
	ServerSettings()
	{
		creative_mode = false;
	}
	bool creative_mode;
};*/

class Server : public con::PeerHandler
{
public:
	/*
		NOTE: Every public method should be thread-safe
	*/
	Server(
		std::string mapsavedir,
		HMParams hm_params,
		MapParams map_params
	);
	~Server();
	void start(unsigned short port);
	void stop();
	// This is mainly a way to pass the time to the server.
	// Actual processing is done in an another thread.
	void step(float dtime);
	// This is run by ServerThread and does the actual processing
	void AsyncRunStep();
	void Receive();
	void ProcessData(u8 *data, u32 datasize, u16 peer_id);

	/*void Send(u16 peer_id, u16 channelnum,
			SharedBuffer<u8> data, bool reliable);*/

	// Environment and Connection must be locked when called
	void SendBlockNoLock(u16 peer_id, MapBlock *block, u8 ver);
	//TODO: Sending of many blocks in a single packet
	
	// Environment and Connection must be locked when called
	//void SendSectorMeta(u16 peer_id, core::list<v2s16> ps, u8 ver);

	core::list<PlayerInfo> getPlayerInfo();
	
private:

	// Virtual methods from con::PeerHandler.
	// As of now, these create and remove clients and players.
	// TODO: Make it possible to leave players on server.
	void peerAdded(con::Peer *peer);
	void deletingPeer(con::Peer *peer, bool timeout);
	
	// Envlock and conlock should be locked when calling these
	void SendObjectData(float dtime);
	void SendPlayerInfos();
	void SendInventory(u16 peer_id);
	// Sends blocks to clients
	void SendBlocks(float dtime);
	
	// When called, connection mutex should be locked
	RemoteClient* getClient(u16 peer_id);
	
	// NOTE: If connection and environment are both to be locked,
	// environment shall be locked first.

	JMutex m_env_mutex;
	Environment m_env;

	JMutex m_con_mutex;
	con::Connection m_con;
	core::map<u16, RemoteClient*> m_clients; // Behind the con mutex

	float m_step_dtime;
	JMutex m_step_dtime_mutex;

	ServerThread m_thread;
	EmergeThread m_emergethread;

	BlockEmergeQueue m_emerge_queue;
	
	friend class EmergeThread;
	friend class RemoteClient;
};

#endif

