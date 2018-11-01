# CN-Simulation
Implementation of a peer to peer network simulation using Omnet++.  Will be used to demonstrate various security issues of blockchain based networks.

Currently simulation creates a mesh of randomly interconnected nodes.  A message is created with a random destination and forwarded until the destination is reached.  The destination node then creates a new message, and the process repeats.  An example with 10 nodes is shown here:
![Example](./sim_example.png)
