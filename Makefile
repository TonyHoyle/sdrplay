test: test.c
	cc -o test test.c -lmir_sdr 

clean:
	rm test
