build: ImageProcessing.c
	mpicc -o imageProcessing ImageProcessing.c -lm

clean:
	rm imageProcessing
