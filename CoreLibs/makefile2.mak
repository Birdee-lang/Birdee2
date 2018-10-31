all: birdee list hash_map
birdee: ..\BirdeeHome\src\birdee.txt
	..\x64\Debug\birdeec.exe -i ..\BirdeeHome\src\birdee.txt -o ..\BirdeeHome\bin\blib\birdee.obj --corelib

list: birdee ..\BirdeeHome\src\list.txt
	..\x64\Debug\birdeec.exe -i ..\BirdeeHome\src\list.txt -o ..\BirdeeHome\bin\blib\list.obj

hash_map: birdee ..\BirdeeHome\src\hash_map.txt
	..\x64\Debug\birdeec.exe -i ..\BirdeeHome\src\hash_map.txt -o ..\BirdeeHome\bin\blib\hash_map.obj

clean:
	del ..\BirdeeHome\bin\blib\*.obj

remake: clean all