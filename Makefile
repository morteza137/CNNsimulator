CXX = g++
CXXFLAGS = -std=c++17 -Wall

SYSTEMC = /usr/local/systemc
INCLUDES = -I$(SYSTEMC)/include -Iinclude
LIBS = -L$(SYSTEMC)/lib-linux64 -lsystemc -lgtest -pthread

BUILD = build

# Automatically build and run tests/test_xxx.cpp
test_%:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) tests/$@.cpp -o $(BUILD)/$@ $(LIBS)
	./$(BUILD)/$@

clean:
	rm -rf $(BUILD)

.PHONY: clean