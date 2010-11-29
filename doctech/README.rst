MineTest Technical Documentation
================================

.. contents::
   :local:

Heightmap
---------

Heightmap is a two dimensional map of terrain height. The heightmap is
organized in two levels, the "master heightmap" and the "sector heightmap".

For details see "`Heightmap Handling <heightmap.rst>`_"

Nodes and objects
-----------------

See "`Elements <elements.rst>`_"

Server
------

"ServerThread" is used (probably) to advance time based changes in the
world. Like movement of the rats or something.

Server::AsyncRunStep()
- step the environment.
- send blocks.
- send object positions.
- tell mapgen thread to generate more blocks.
- save the map.

Server::Receive()

"EmergeThread" is used to load or generate mapblocks and send them to the
client(s).

The main thread has nothing useful to do in the dedicated server and is used
to run the client ???.
