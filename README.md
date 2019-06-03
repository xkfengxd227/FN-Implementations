# FN-Implementations
Implementations of the Furthest Neighbor Datastructure used for the experiments in "Approximate Furthest Neighbor in High Dimensions"


## After download the original codes, encountered some build error, like 
some build error, like 
```/lib/x86_64-linux-gnu/libc.so.6: error adding symbols: Bad value
```
to fix it, we need
	- first create folder [bin] [lib] in the root folder
	- then add #include <errno.h> in the src/indexes/furthest/furthest.c
	- build, OK
