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

#include "test.h"
#include "common_irrlicht.h"
#include "debug.h"
#include "map.h"
#include "player.h"
#include "main.h"
#include "heightmap.h"
#include "socket.h"
#include "connection.h"
#include "utility.h"
#include "serialization.h"
#include "voxel.h"
#include <sstream>

#include <unistd.h>
#define sleep_ms(x) usleep(x*1000)

/*
	Asserts that the exception occurs
*/
#define EXCEPTION_CHECK(EType, code)\
{\
	bool exception_thrown = false;\
	try{ code; }\
	catch(EType &e) { exception_thrown = true; }\
	assert(exception_thrown);\
}

struct TestUtilities
{
	void Run()
	{
		/*dstream<<"wrapDegrees(100.0) = "<<wrapDegrees(100.0)<<std::endl;
		dstream<<"wrapDegrees(720.5) = "<<wrapDegrees(720.5)<<std::endl;
		dstream<<"wrapDegrees(-0.5) = "<<wrapDegrees(-0.5)<<std::endl;*/
		assert(fabs(wrapDegrees(100.0) - 100.0) < 0.001);
		assert(fabs(wrapDegrees(720.5) - 0.5) < 0.001);
		assert(fabs(wrapDegrees(-0.5) - (-0.5)) < 0.001);
		assert(fabs(wrapDegrees(-365.5) - (-5.5)) < 0.001);
		assert(lowercase("Foo bAR") == "foo bar");
		assert(is_yes("YeS") == true);
		assert(is_yes("") == false);
		assert(is_yes("FAlse") == false);
	}
};
		
struct TestCompress
{
	void Run()
	{
		SharedBuffer<u8> fromdata(4);
		fromdata[0]=1;
		fromdata[1]=5;
		fromdata[2]=5;
		fromdata[3]=1;
		
		std::ostringstream os(std::ios_base::binary);
		compress(fromdata, os, 0);

		std::string str_out = os.str();
		
		dstream<<"str_out.size()="<<str_out.size()<<std::endl;
		dstream<<"TestCompress: 1,5,5,1 -> ";
		for(u32 i=0; i<str_out.size(); i++)
		{
			dstream<<(u32)str_out[i]<<",";
		}
		dstream<<std::endl;

		assert(str_out.size() == 10);

		assert(str_out[0] == 0);
		assert(str_out[1] == 0);
		assert(str_out[2] == 0);
		assert(str_out[3] == 4);
		assert(str_out[4] == 0);
		assert(str_out[5] == 1);
		assert(str_out[6] == 1);
		assert(str_out[7] == 5);
		assert(str_out[8] == 0);
		assert(str_out[9] == 1);

		std::istringstream is(str_out, std::ios_base::binary);
		std::ostringstream os2(std::ios_base::binary);

		decompress(is, os2, 0);
		std::string str_out2 = os2.str();

		dstream<<"decompress: ";
		for(u32 i=0; i<str_out2.size(); i++)
		{
			dstream<<(u32)str_out2[i]<<",";
		}
		dstream<<std::endl;

		assert(str_out2.size() == fromdata.getSize());

		for(u32 i=0; i<str_out2.size(); i++)
		{
			assert(str_out2[i] == fromdata[i]);
		}
	}
};

struct TestMapNode
{
	void Run()
	{
		MapNode n;

		// Default values
		assert(n.d == MATERIAL_AIR);
		assert(n.getLight() == 0);
		
		// Transparency
		n.d = MATERIAL_AIR;
		assert(n.light_propagates() == true);
		n.d = 0;
		assert(n.light_propagates() == false);
	}
};

struct TestVoxelManipulator
{
	void Run()
	{
		VoxelArea a(v3s16(-1,-1,-1), v3s16(1,1,1));
		assert(a.index(0,0,0) == 1*3*3 + 1*3 + 1);
		assert(a.index(-1,-1,-1) == 0);

		VoxelManipulator v;

		v.print(dstream);

		dstream<<"*** Setting (-1,0,-1)=2 ***"<<std::endl;

		//v[v3s16(-1,0,-1)] = MapNode(2);
		v[v3s16(-1,0,-1)].d = 2;

		v.print(dstream);

 		assert(v[v3s16(-1,0,-1)].d == 2);

		dstream<<"*** Reading from inexistent (0,0,-1) ***"<<std::endl;

		assert(v[v3s16(0,0,-1)].d == MATERIAL_IGNORE);

		v.print(dstream);

		dstream<<"*** Adding area ***"<<std::endl;

		v.addArea(a);
		
		v.print(dstream);

		assert(v[v3s16(-1,0,-1)].d == 2);
		assert(v[v3s16(0,1,1)].d == MATERIAL_IGNORE);
		
	}
};

struct TestMapBlock
{
	class TC : public NodeContainer
	{
	public:

		MapNode node;
		bool position_valid;
		core::list<v3s16> validity_exceptions;

		TC()
		{
			position_valid = true;
		}

		virtual bool isValidPosition(v3s16 p)
		{
			//return position_valid ^ (p==position_valid_exception);
			bool exception = false;
			for(core::list<v3s16>::Iterator i=validity_exceptions.begin();
					i != validity_exceptions.end(); i++)
			{
				if(p == *i)
				{
					exception = true;
					break;
				}
			}
			return exception ? !position_valid : position_valid;
		}

		virtual MapNode getNode(v3s16 p)
		{
			if(isValidPosition(p) == false)
				throw InvalidPositionException();
			return node;
		}

		virtual void setNode(v3s16 p, MapNode & n)
		{
			if(isValidPosition(p) == false)
				throw InvalidPositionException();
		};

		virtual u16 nodeContainerId() const
		{
			return 666;
		}
	};

	void Run()
	{
		TC parent;
		
		MapBlock b(&parent, v3s16(1,1,1));
		v3s16 relpos(MAP_BLOCKSIZE, MAP_BLOCKSIZE, MAP_BLOCKSIZE);

		assert(b.getPosRelative() == relpos);

		assert(b.getBox().MinEdge.X == MAP_BLOCKSIZE);
		assert(b.getBox().MaxEdge.X == MAP_BLOCKSIZE*2-1);
		assert(b.getBox().MinEdge.Y == MAP_BLOCKSIZE);
		assert(b.getBox().MaxEdge.Y == MAP_BLOCKSIZE*2-1);
		assert(b.getBox().MinEdge.Z == MAP_BLOCKSIZE);
		assert(b.getBox().MaxEdge.Z == MAP_BLOCKSIZE*2-1);
		
		assert(b.isValidPosition(v3s16(0,0,0)) == true);
		assert(b.isValidPosition(v3s16(-1,0,0)) == false);
		assert(b.isValidPosition(v3s16(-1,-142,-2341)) == false);
		assert(b.isValidPosition(v3s16(-124,142,2341)) == false);
		assert(b.isValidPosition(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1)) == true);
		assert(b.isValidPosition(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE,MAP_BLOCKSIZE-1)) == false);

		/*
			TODO: this method should probably be removed
			if the block size isn't going to be set variable
		*/
		/*assert(b.getSizeNodes() == v3s16(MAP_BLOCKSIZE,
				MAP_BLOCKSIZE, MAP_BLOCKSIZE));*/
		
		// Changed flag should be initially set
		assert(b.getChangedFlag() == true);
		b.resetChangedFlag();
		assert(b.getChangedFlag() == false);

		// All nodes should have been set to
		// .d=MATERIAL_AIR and .getLight() = 0
		for(u16 z=0; z<MAP_BLOCKSIZE; z++)
			for(u16 y=0; y<MAP_BLOCKSIZE; y++)
				for(u16 x=0; x<MAP_BLOCKSIZE; x++){
					assert(b.getNode(v3s16(x,y,z)).d == MATERIAL_AIR);
					assert(b.getNode(v3s16(x,y,z)).getLight() == 0);
				}
		
		/*
			Parent fetch functions
		*/
		parent.position_valid = false;
		parent.node.d = 5;

		MapNode n;
		
		// Positions in the block should still be valid
		assert(b.isValidPositionParent(v3s16(0,0,0)) == true);
		assert(b.isValidPositionParent(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1)) == true);
		n = b.getNodeParent(v3s16(0,MAP_BLOCKSIZE-1,0));
		assert(n.d == MATERIAL_AIR);

		// ...but outside the block they should be invalid
		assert(b.isValidPositionParent(v3s16(-121,2341,0)) == false);
		assert(b.isValidPositionParent(v3s16(-1,0,0)) == false);
		assert(b.isValidPositionParent(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE)) == false);
		
		{
			bool exception_thrown = false;
			try{
				// This should throw an exception
				b.getNodeParent(v3s16(0,0,-1));
			}
			catch(InvalidPositionException &e)
			{
				exception_thrown = true;
			}
			assert(exception_thrown);
		}

		parent.position_valid = true;
		// Now the positions outside should be valid
		assert(b.isValidPositionParent(v3s16(-121,2341,0)) == true);
		assert(b.isValidPositionParent(v3s16(-1,0,0)) == true);
		assert(b.isValidPositionParent(v3s16(MAP_BLOCKSIZE-1,MAP_BLOCKSIZE-1,MAP_BLOCKSIZE)) == true);
		n = b.getNodeParent(v3s16(0,0,MAP_BLOCKSIZE));
		assert(n.d == 5);

		/*
			Set a node
		*/
		v3s16 p(1,2,0);
		n.d = 4;
		b.setNode(p, n);
		assert(b.getNode(p).d == 4);
		assert(b.getNodeMaterial(p) == 4);
		assert(b.getNodeMaterial(v3s16(-1,-1,0)) == 5);
		
		/*
			propagateSunlight()
		*/
		// Set lighting of all nodes to 0
		for(u16 z=0; z<MAP_BLOCKSIZE; z++){
			for(u16 y=0; y<MAP_BLOCKSIZE; y++){
				for(u16 x=0; x<MAP_BLOCKSIZE; x++){
					MapNode n = b.getNode(v3s16(x,y,z));
					n.setLight(0);
					b.setNode(v3s16(x,y,z), n);
				}
			}
		}
		{
			/*
				Check how the block handles being a lonely sky block
			*/
			parent.position_valid = true;
			b.setIsUnderground(false);
			parent.node.d = MATERIAL_AIR;
			parent.node.setLight(LIGHT_SUN);
			core::map<v3s16, bool> light_sources;
			// The bottom block is invalid, because we have a shadowing node
			assert(b.propagateSunlight(light_sources) == false);
			assert(b.getNode(v3s16(1,4,0)).getLight() == LIGHT_SUN);
			assert(b.getNode(v3s16(1,3,0)).getLight() == LIGHT_SUN);
			assert(b.getNode(v3s16(1,2,0)).getLight() == 0);
			assert(b.getNode(v3s16(1,1,0)).getLight() == 0);
			assert(b.getNode(v3s16(1,0,0)).getLight() == 0);
			assert(b.getNode(v3s16(1,2,3)).getLight() == LIGHT_SUN);
			assert(b.getFaceLight(p, v3s16(0,1,0)) == LIGHT_SUN);
			assert(b.getFaceLight(p, v3s16(0,-1,0)) == 0);
			// According to MapBlock::getFaceLight,
			// The face on the z+ side should have double-diminished light
			assert(b.getFaceLight(p, v3s16(0,0,1)) == diminish_light(diminish_light(LIGHT_MAX)));
		}
		/*
			Check how the block handles being in between blocks with some non-sunlight
			while being underground
		*/
		{
			// Make neighbours to exist and set some non-sunlight to them
			parent.position_valid = true;
			b.setIsUnderground(true);
			parent.node.setLight(LIGHT_MAX/2);
			core::map<v3s16, bool> light_sources;
			// The block below should be valid because there shouldn't be
			// sunlight in there either
			assert(b.propagateSunlight(light_sources) == true);
			// Should not touch nodes that are not affected (that is, all of them)
			//assert(b.getNode(v3s16(1,2,3)).getLight() == LIGHT_SUN);
			// Should set light of non-sunlighted blocks to 0.
			assert(b.getNode(v3s16(1,2,3)).getLight() == 0);
		}
		/*
			Set up a situation where:
			- There is only air in this block
			- There is a valid non-sunlighted block at the bottom, and
			- Invalid blocks elsewhere.
			- the block is not underground.

			This should result in bottom block invalidity
		*/
		{
			b.setIsUnderground(false);
			// Clear block
			for(u16 z=0; z<MAP_BLOCKSIZE; z++){
				for(u16 y=0; y<MAP_BLOCKSIZE; y++){
					for(u16 x=0; x<MAP_BLOCKSIZE; x++){
						MapNode n;
						n.d = MATERIAL_AIR;
						n.setLight(0);
						b.setNode(v3s16(x,y,z), n);
					}
				}
			}
			// Make neighbours invalid
			parent.position_valid = false;
			// Add exceptions to the top of the bottom block
			for(u16 x=0; x<MAP_BLOCKSIZE; x++)
			for(u16 z=0; z<MAP_BLOCKSIZE; z++)
			{
				parent.validity_exceptions.push_back(v3s16(MAP_BLOCKSIZE+x, MAP_BLOCKSIZE-1, MAP_BLOCKSIZE+z));
			}
			// Lighting value for the valid nodes
			parent.node.setLight(LIGHT_MAX/2);
			core::map<v3s16, bool> light_sources;
			// Bottom block is not valid
			assert(b.propagateSunlight(light_sources) == false);
		}
	}
};

struct TestMapSector
{
	class TC : public NodeContainer
	{
	public:

		MapNode node;
		bool position_valid;

		TC()
		{
			position_valid = true;
		}

		virtual bool isValidPosition(v3s16 p)
		{
			return position_valid;
		}

		virtual MapNode getNode(v3s16 p)
		{
			if(position_valid == false)
				throw InvalidPositionException();
			return node;
		}

		virtual void setNode(v3s16 p, MapNode & n)
		{
			if(position_valid == false)
				throw InvalidPositionException();
		};
		
		virtual u16 nodeContainerId() const
		{
			return 666;
		}
	};
	
	void Run()
	{
		TC parent;
		parent.position_valid = false;
		
		// Create one with no heightmaps
		ServerMapSector sector(&parent, v2s16(1,1), 0);
		//ConstantGenerator *dummyheightmap = new ConstantGenerator();
		//sector->setHeightmap(dummyheightmap);
		
		EXCEPTION_CHECK(InvalidPositionException, sector.getBlockNoCreate(0));
		EXCEPTION_CHECK(InvalidPositionException, sector.getBlockNoCreate(1));

		MapBlock * bref = sector.createBlankBlock(-2);
		
		EXCEPTION_CHECK(InvalidPositionException, sector.getBlockNoCreate(0));
		assert(sector.getBlockNoCreate(-2) == bref);
		
		//TODO: Check for AlreadyExistsException

		/*bool exception_thrown = false;
		try{
			sector.getBlock(0);
		}
		catch(InvalidPositionException &e){
			exception_thrown = true;
		}
		assert(exception_thrown);*/

	}
};

struct TestHeightmap
{
	void TestSingleFixed()
	{
		const s16 BS1 = 4;
		OneChildHeightmap hm1(BS1);
		
		// Test that it is filled with < GROUNDHEIGHT_VALID_MINVALUE
		for(s16 y=0; y<=BS1; y++){
			for(s16 x=0; x<=BS1; x++){
				v2s16 p(x,y);
				assert(hm1.m_child.getGroundHeight(p)
					< GROUNDHEIGHT_VALID_MINVALUE);
			}
		}

		hm1.m_child.setGroundHeight(v2s16(1,0), 2.0);
		//hm1.m_child.print();
		assert(fabs(hm1.getGroundHeight(v2s16(1,0))-2.0)<0.001);
		hm1.setGroundHeight(v2s16(0,1), 3.0);
		assert(fabs(hm1.m_child.getGroundHeight(v2s16(0,1))-3.0)<0.001);
		
		// Fill with -1.0
		for(s16 y=0; y<=BS1; y++){
			for(s16 x=0; x<=BS1; x++){
				v2s16 p(x,y);
				hm1.m_child.setGroundHeight(p, -1.0);
			}
		}

		f32 corners[] = {0.0, 0.0, 1.0, 1.0};
		hm1.m_child.generateContinued(0.0, 0.0, corners);
		
		hm1.m_child.print();
		assert(fabs(hm1.m_child.getGroundHeight(v2s16(1,0))-0.2)<0.05);
		assert(fabs(hm1.m_child.getGroundHeight(v2s16(4,3))-0.7)<0.05);
		assert(fabs(hm1.m_child.getGroundHeight(v2s16(4,4))-1.0)<0.05);
	}

	void TestUnlimited()
	{
		//g_heightmap_debugprint = true;
		const s16 BS1 = 4;
		UnlimitedHeightmap hm1(BS1,
				new ConstantGenerator(0.0),
				new ConstantGenerator(0.0),
				new ConstantGenerator(5.0));
		// Go through it so it generates itself
		for(s16 y=0; y<=BS1; y++){
			for(s16 x=0; x<=BS1; x++){
				v2s16 p(x,y);
				hm1.getGroundHeight(p);
			}
		}
		// Print it
		dstream<<"UnlimitedHeightmap hm1:"<<std::endl;
		hm1.print();
		
		dstream<<"testing UnlimitedHeightmap set/get"<<std::endl;
		v2s16 p1(0,3);
		f32 v1(234.01);
		// Get first heightmap and try setGroundHeight
		FixedHeightmap * href = hm1.getHeightmap(v2s16(0,0));
		href->setGroundHeight(p1, v1);
		// Read from UnlimitedHeightmap
		assert(fabs(hm1.getGroundHeight(p1)-v1)<0.001);
	}
	
	void Random()
	{
		dstream<<"Running random code (get a human to check this)"<<std::endl;
		dstream<<"rand() values: ";
		for(u16 i=0; i<5; i++)
			dstream<<(u16)rand()<<" ";
		dstream<<std::endl;

		const s16 BS1 = 8;
		UnlimitedHeightmap hm1(BS1,
				new ConstantGenerator(10.0),
				new ConstantGenerator(0.3),
				new ConstantGenerator(0.0));

		// Force hm1 to generate a some heightmap
		hm1.getGroundHeight(v2s16(0,0));
		hm1.getGroundHeight(v2s16(0,BS1));
		/*hm1.getGroundHeight(v2s16(BS1,-1));
		hm1.getGroundHeight(v2s16(BS1-1,-1));*/
		hm1.print();

		// Get the (0,0) and (1,0) heightmaps
		/*FixedHeightmap * hr00 = hm1.getHeightmap(v2s16(0,0));
		FixedHeightmap * hr01 = hm1.getHeightmap(v2s16(1,0));
		f32 corners[] = {1.0, 1.0, 1.0, 1.0};
		hr00->generateContinued(0.0, 0.0, corners);
		hm1.print();*/

		//assert(0);
	}

	void Run()
	{
		//srand(7); // Get constant random
		srand(time(0)); // Get better random

		TestSingleFixed();
		TestUnlimited();
		Random();
	}
};

struct TestSocket
{
	void Run()
	{
		const int port = 30003;
		UDPSocket socket;
		socket.Bind(port);

		const char sendbuffer[] = "hello world!";
		socket.Send(Address(127,0,0,1,port), sendbuffer, sizeof(sendbuffer));

		sleep_ms(50);

		char rcvbuffer[256];
		memset(rcvbuffer, 0, sizeof(rcvbuffer));
		Address sender;
		for(;;)
		{
			int bytes_read = socket.Receive(sender, rcvbuffer, sizeof(rcvbuffer));
			if(bytes_read < 0)
				break;
		}
		//FIXME: This fails on some systems
		assert(strncmp(sendbuffer, rcvbuffer, sizeof(sendbuffer))==0);
		assert(sender.getAddress() == Address(127,0,0,1, 0).getAddress());
	}
};

struct TestConnection
{
	void TestHelpers()
	{
		/*
			Test helper functions
		*/

		// Some constants for testing
		u32 proto_id = 0x12345678;
		u16 peer_id = 123;
		u8 channel = 2;
		SharedBuffer<u8> data1(1);
		data1[0] = 100;
		Address a(127,0,0,1, 10);
		u16 seqnum = 34352;

		con::BufferedPacket p1 = con::makePacket(a, data1,
				proto_id, peer_id, channel);
		/*
			We should now have a packet with this data:
			Header:
				[0] u32 protocol_id
				[4] u16 sender_peer_id
				[6] u8 channel
			Data:
				[7] u8 data1[0]
		*/
		assert(readU32(&p1.data[0]) == proto_id);
		assert(readU16(&p1.data[4]) == peer_id);
		assert(readU8(&p1.data[6]) == channel);
		assert(readU8(&p1.data[7]) == data1[0]);
		
		//dstream<<"initial data1[0]="<<((u32)data1[0]&0xff)<<std::endl;

		SharedBuffer<u8> p2 = con::makeReliablePacket(data1, seqnum);

		/*dstream<<"p2.getSize()="<<p2.getSize()<<", data1.getSize()="
				<<data1.getSize()<<std::endl;
		dstream<<"readU8(&p2[3])="<<readU8(&p2[3])
				<<" p2[3]="<<((u32)p2[3]&0xff)<<std::endl;
		dstream<<"data1[0]="<<((u32)data1[0]&0xff)<<std::endl;*/

		assert(p2.getSize() == 3 + data1.getSize());
		assert(readU8(&p2[0]) == TYPE_RELIABLE);
		assert(readU16(&p2[1]) == seqnum);
		assert(readU8(&p2[3]) == data1[0]);
	}

	struct Handler : public con::PeerHandler
	{
		Handler(const char *a_name)
		{
			count = 0;
			last_id = 0;
			name = a_name;
		}
		void peerAdded(con::Peer *peer)
		{
			dstream<<"Handler("<<name<<")::peerAdded(): "
					"id="<<peer->id<<std::endl;
			last_id = peer->id;
			count++;
		}
		void deletingPeer(con::Peer *peer, bool timeout)
		{
			dstream<<"Handler("<<name<<")::deletingPeer(): "
					"id="<<peer->id
					<<", timeout="<<timeout<<std::endl;
			last_id = peer->id;
			count--;
		}

		s32 count;
		u16 last_id;
		const char *name;
	};

	void Run()
	{
		TestHelpers();

		/*
			Test some real connections
		*/
		u32 proto_id = 0xad26846a;

		Handler hand_server("server");
		Handler hand_client("client");
		
		dstream<<"** Creating server Connection"<<std::endl;
		con::Connection server(proto_id, 512, 5.0, &hand_server);
		server.Serve(30001);
		
		dstream<<"** Creating client Connection"<<std::endl;
		con::Connection client(proto_id, 512, 5.0, &hand_client);

		assert(hand_server.count == 0);
		assert(hand_client.count == 0);
		
		sleep_ms(50);
		
		Address server_address(127,0,0,1, 30001);
		dstream<<"** running client.Connect()"<<std::endl;
		client.Connect(server_address);

		sleep_ms(50);
		
		// Client should have added server now
		assert(hand_client.count == 1);
		assert(hand_client.last_id == 1);
		// But server should not have added client
		assert(hand_server.count == 0);

		try
		{
			u16 peer_id;
			u8 data[100];
			dstream<<"** running server.Receive()"<<std::endl;
			u32 size = server.Receive(peer_id, data, 100);
			dstream<<"** Server received: peer_id="<<peer_id
					<<", size="<<size
					<<std::endl;
		}
		catch(con::NoIncomingDataException &e)
		{
			// No actual data received, but the client has
			// probably been connected
		}
		
		// Client should be the same
		assert(hand_client.count == 1);
		assert(hand_client.last_id == 1);
		// Server should have the client
		assert(hand_server.count == 1);
		assert(hand_server.last_id == 2);
		
		//sleep_ms(50);

		while(client.Connected() == false)
		{
			try
			{
				u16 peer_id;
				u8 data[100];
				dstream<<"** running client.Receive()"<<std::endl;
				u32 size = client.Receive(peer_id, data, 100);
				dstream<<"** Client received: peer_id="<<peer_id
						<<", size="<<size
						<<std::endl;
			}
			catch(con::NoIncomingDataException &e)
			{
			}
			sleep_ms(50);
		}

		sleep_ms(50);
		
		try
		{
			u16 peer_id;
			u8 data[100];
			dstream<<"** running server.Receive()"<<std::endl;
			u32 size = server.Receive(peer_id, data, 100);
			dstream<<"** Server received: peer_id="<<peer_id
					<<", size="<<size
					<<std::endl;
		}
		catch(con::NoIncomingDataException &e)
		{
		}

		{
			/*u8 data[] = "Hello World!";
			u32 datasize = sizeof(data);*/
			SharedBuffer<u8> data = SharedBufferFromString("Hello World!");

			dstream<<"** running client.Send()"<<std::endl;
			client.Send(PEER_ID_SERVER, 0, data, true);

			sleep_ms(50);

			u16 peer_id;
			u8 recvdata[100];
			dstream<<"** running server.Receive()"<<std::endl;
			u32 size = server.Receive(peer_id, recvdata, 100);
			dstream<<"** Server received: peer_id="<<peer_id
					<<", size="<<size
					<<", data="<<*data
					<<std::endl;
			assert(memcmp(*data, recvdata, data.getSize()) == 0);
		}
		
		u16 peer_id_client = 2;

		{
			/*
				Send consequent packets in different order
			*/
			//u8 data1[] = "hello1";
			//u8 data2[] = "hello2";
			SharedBuffer<u8> data1 = SharedBufferFromString("hello1");
			SharedBuffer<u8> data2 = SharedBufferFromString("Hello2");

			dstream<<"*** Sending packets in wrong order (2,1,2)"
					<<std::endl;
			
			u8 chn = 0;
			con::Channel *ch = &server.GetPeer(peer_id_client)->channels[chn];
			u16 sn = ch->next_outgoing_seqnum;
			ch->next_outgoing_seqnum = sn+1;
			server.Send(peer_id_client, chn, data2, true);
			ch->next_outgoing_seqnum = sn;
			server.Send(peer_id_client, chn, data1, true);
			ch->next_outgoing_seqnum = sn+1;
			server.Send(peer_id_client, chn, data2, true);

			sleep_ms(50);

			dstream<<"*** Receiving the packets"<<std::endl;

			u16 peer_id;
			u8 recvdata[20];
			u32 size;

			dstream<<"** running client.Receive()"<<std::endl;
			peer_id = 132;
			size = client.Receive(peer_id, recvdata, 20);
			dstream<<"** Client received: peer_id="<<peer_id
					<<", size="<<size
					<<", data="<<recvdata
					<<std::endl;
			assert(size == data1.getSize());
			assert(memcmp(*data1, recvdata, data1.getSize()) == 0);
			assert(peer_id == PEER_ID_SERVER);
			
			dstream<<"** running client.Receive()"<<std::endl;
			peer_id = 132;
			size = client.Receive(peer_id, recvdata, 20);
			dstream<<"** Client received: peer_id="<<peer_id
					<<", size="<<size
					<<", data="<<recvdata
					<<std::endl;
			assert(size == data2.getSize());
			assert(memcmp(*data2, recvdata, data2.getSize()) == 0);
			assert(peer_id == PEER_ID_SERVER);
			
			bool got_exception = false;
			try
			{
				dstream<<"** running client.Receive()"<<std::endl;
				peer_id = 132;
				size = client.Receive(peer_id, recvdata, 20);
				dstream<<"** Client received: peer_id="<<peer_id
						<<", size="<<size
						<<", data="<<recvdata
						<<std::endl;
			}
			catch(con::NoIncomingDataException &e)
			{
				dstream<<"** No incoming data for client"<<std::endl;
				got_exception = true;
			}
			assert(got_exception);
		}
		{
			//u8 data1[1100];
			SharedBuffer<u8> data1(1100);
			for(u16 i=0; i<1100; i++){
				data1[i] = i/4;
			}

			dstream<<"Sending data (size="<<1100<<"):";
			for(int i=0; i<1100 && i<20; i++){
				if(i%2==0) printf(" ");
				printf("%.2X", ((int)((const char*)*data1)[i])&0xff);
			}
			if(1100>20)
				dstream<<"...";
			dstream<<std::endl;
			
			server.Send(peer_id_client, 0, data1, true);

			sleep_ms(50);
			
			u8 recvdata[2000];
			dstream<<"** running client.Receive()"<<std::endl;
			u16 peer_id = 132;
			u16 size = client.Receive(peer_id, recvdata, 2000);
			dstream<<"** Client received: peer_id="<<peer_id
					<<", size="<<size
					<<std::endl;

			dstream<<"Received data (size="<<size<<"):";
			for(int i=0; i<size && i<20; i++){
				if(i%2==0) printf(" ");
				printf("%.2X", ((int)((const char*)recvdata)[i])&0xff);
			}
			if(size>20)
				dstream<<"...";
			dstream<<std::endl;

			assert(memcmp(*data1, recvdata, data1.getSize()) == 0);
			assert(peer_id == PEER_ID_SERVER);
		}
		
		// Check peer handlers
		assert(hand_client.count == 1);
		assert(hand_client.last_id == 1);
		assert(hand_server.count == 1);
		assert(hand_server.last_id == 2);
		
		//assert(0);
	}
};

#define TEST(X)\
{\
	X x;\
	dstream<<"Running " #X <<std::endl;\
	x.Run();\
}

void run_tests()
{
	dstream<<"run_tests() started"<<std::endl;
	TEST(TestUtilities);
	TEST(TestCompress);
	TEST(TestMapNode);
	TEST(TestVoxelManipulator);
	TEST(TestMapBlock);
	TEST(TestMapSector);
	TEST(TestHeightmap);
	if(INTERNET_SIMULATOR == false){
		TEST(TestSocket);
		dout_con<<"=== BEGIN RUNNING UNIT TESTS FOR CONNECTION ==="<<std::endl;
		TEST(TestConnection);
		dout_con<<"=== END RUNNING UNIT TESTS FOR CONNECTION ==="<<std::endl;
	}
	dstream<<"run_tests() passed"<<std::endl;
}

