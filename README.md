#WSN
##introduction
This project is aimed to build WSN Middle ware for WSN (wireless sensor network) providing
interfaces to assemble the nodes data by subscribing and unsubscribe the node. The node can
be classified into two types, the sink and the sensor node. The Sink is able to subscribe a
certain type of sensor node and force these sensors nodes assemble data and send back to
the sink through multi-hops routing. The sink is also able to unsubscribe a type of sensor node,
so that it will be able to stop receiving data from a certain type of sensor node. The middle ware
should also support the node arriving and node leaving, new routes will be created while the
node joining and failing. Basically we mainly provide three types of interfaces, which are
Subscribe, UN-Subscribe and Publish. At the same time, the middleware should decrease the
reliability(e.g.) message loss and extreme situation such as a very dense network. In that case,
the routing will be more unreliable and interruption will happen very often. The middle ware
should be able to run reasonable delivery ability in such a very dense network.
##tips
The HEAT.c and Sensor.c are the same code with different pre-configurations
in the init(node), you can rename the file to the type of sensor you want.
