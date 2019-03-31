luarure.so: luarure.c
	$(CC) -Wall -shared -fPIC -g luarure.c -o luarure.so -lrure -lm