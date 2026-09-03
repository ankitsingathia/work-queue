CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -pthread

wqueue: main.cpp wqueue.h pool.h
	$(CXX) $(CXXFLAGS) main.cpp -o wqueue

# Build with ThreadSanitizer to check for data races.
tsan: main.cpp wqueue.h pool.h
	$(CXX) -std=c++17 -g -fsanitize=thread -pthread main.cpp -o wqueue_tsan

clean:
	rm -f wqueue wqueue_tsan

.PHONY: tsan clean
