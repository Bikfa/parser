parser: parser.cpp
    g++ -std=c++11 -o parser parser.cpp

clean:
    rm -f parser