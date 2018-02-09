Ardino library to simplify access to Eseye anynet secure click modules.

The Eseye anynet-secure AT-interface is very simple to use directly.
This library attempts to make it even simpler.

Subscribing to a topic is as simple as calling subscribe(topic, callback). 
The callback function gets called when a message is received to the subscribed topic. 

Publishing requires a call to pubreg(topic) which returns a publish index.
To publish a message call publish(pubidx, data, datalen).

Ensure poll() is called inside loop().


The included example is quite complex but shows much of the library functionality.
Sleep support is currently in development and untested.

