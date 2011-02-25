all: p2 p3 p4

p2: p23.c board.c
	$(CC) -o $@ $^ -lpthread -lrt

p3: p3.c 
	$(CC) -o $@ p3.c -lpthread -lrt -lm

p4: p4.cu
	nvcc -o p4 p4.cu

clean:
	rm p2 p3 p4
