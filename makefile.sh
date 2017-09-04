#!/bin/bash
clear
clear
echo "[1/10] Remove old files"
rm -f *.o *.so *.a myprogram genericDataStructure
if [ $? -ne 0 ];then
   exit 1
fi
echo "[2/10] Compile generic data structure"
g++ -c genericDataStructure.c -o genericDataStructure.o
if [ $? -ne 0 ];then
   exit 1
fi
echo "[3/10] Create static library -> libgenericDS"
ar -r -s -c libgenericDS.a genericDataStructure.o
echo "[4/10] Compile NUMAbb"
g++ -static -c NUMAbb.c -o NUMAbb.o -L. -lgenericDS
if [ $? -ne 0 ];then
   exit 1
fi
echo "[5/10] Create static library -> libNUMAbb"
ar -r -s -c libNUMAbb.a NUMAbb.o
echo "[6/10] Compile generateCommands"
g++ -c generateCommands.c -o generateCommands.o
if [ $? -ne 0 ];then
   exit 1
fi
echo "[7/10] Create static library -> libgenerateCommands"
ar -r -s -c libgenerateCommands.a generateCommands.o
echo "[8/10] Merge everything together"
g++ main.c -o myprogram -pthread -lnuma -lutil -L. -lNUMAbb -lgenericDS -lgenerateCommands
if [ $? -ne 0 ];then
   exit 1
fi
echo "[9/10] Remove temp files"
rm -f *.o *.so *.a
if [ $? -ne 0 ];then
   exit 1
fi
echo "[10/10] Run our program"
./myprogram

# -Wall -Wextra -Werror -pedantic -std=c++11
