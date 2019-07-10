Lewis:Lewis.cc
	g++ -std=c++11 -o Lewis Lewis.cc -Ispeech -ljsoncpp -lcurl -lcrypto -lpthread

.PHONY:clean
clean:
	rm -f Lewis
