#!/bin/bash
make clean
make 
./bin/d7fs disk.hex smount -f -s

#./bin/d7fs newdisk disk.hex 20

