parser: parser.cpp
	g++ main.o parser.o -o parser -std=c++11
parser.o: parser.cpp
	g++ -c parser.cpp -std=c++11
clean:
    rm -f parser
