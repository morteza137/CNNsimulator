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

test_pe:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) tests/test_pe.cpp src/PE.cpp -o $(BUILD)/test_pe $(LIBS)
	./$(BUILD)/test_pe

test_pearray:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tests/test_pearray.cpp \
		src/PE.cpp \
		src/PEArray.cpp \
		-o $(BUILD)/test_pearray $(LIBS)
	./$(BUILD)/test_pearray

clean:
	rm -rf $(BUILD)

.PHONY: clean