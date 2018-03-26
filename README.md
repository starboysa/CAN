# CAN
CAN : Components As Nodes

![Alt text](ReadmeImages/basic.png?raw=true "Basic")

CAN is a statically typed visual scripting language that can be used to extend any software that uses component based architecture, but was made and tested with game engines in mind.
By targetting a specific domain we can consider language features that will improve iteration time on components.
CAN can be interpretted or transpiled into C++ to be integrated in your software however you want.  The goal here is to allow CAN scripts to be preformant enough to ship in a final product.

# Current State
It isn't usuable, not yet.
First I'm focusing on the functional side of the language, the actual nodes that are components are still quite a ways away.

![Alt text](ReadmeImages/complex.png?raw=true "Complex")

This is the extent of the lanuage.  Stitching together literals, math, and printing nodes.  As mentioned this can be interpreted or just outputed to C++ as shown.

# Features in Progress
* Node Components
* C++ API
* Data Serialization
* Data Versioning
  * Each Node will have a data version and each member on the node will have a data version that they're a part of.  When incrementing the data version on the node you can mark a data member as part of a previous data version and supply a conversion function so that you can easily deserialize old data layouts and update them by just loading them into your CAN instance.
