CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -pthread

wqueue: main.cpp wqueue.h pool.h
	$(CXX) $(CXXFLAGS) main.cpp -o wqueue

# Build with ThreadSanitizer to check for data races.
tsan: main.cpp wqueue.h pool.h
	$(CXX) -std=c++17 -g -fsanitize=thread -pthread main.cpp -o wqueue_tsan

# Unit tests for the queue and the pool.
test: test.cpp wqueue.h pool.h
	$(CXX) $(CXXFLAGS) test.cpp -o wqueue_test
	./wqueue_test

# The same tests under ThreadSanitizer.
test-tsan: test.cpp wqueue.h pool.h
	$(CXX) -std=c++17 -g -fsanitize=thread -pthread test.cpp -o wqueue_test_tsan
	./wqueue_test_tsan

clean:
	rm -f wqueue wqueue_tsan wqueue_test wqueue_test_tsan

.PHONY: tsan test test-tsan clean
