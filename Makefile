build: cotton-runtime.cpp
	g++ -c -Wall -fpic cotton-runtime.cpp -lpthread -g
	g++ -shared cotton-runtime.o -o libcotton.so -lpthread -g
	sudo cp libcotton.so /usr/lib
	g++ -L. -o NQueens nqueens.cpp -lcotton -lpthread -g

clean: cotton-runtime.o libcotton.so NQueens
	rm cotton-runtime.o libcotton.so NQueens

	
