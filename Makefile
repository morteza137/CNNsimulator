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

test_dataflow:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tests/test_dataflow.cpp \
		src/PE.cpp \
		src/PEArray.cpp \
		src/Dataflow.cpp \
		-o $(BUILD)/test_dataflow $(LIBS)
	./$(BUILD)/test_dataflow

test_dataflowbench:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tests/test_dataflowbench.cpp \
		src/Dataflow.cpp \
		src/PEArray.cpp \
		src/PE.cpp \
		-o $(BUILD)/test_dataflowbench $(LIBS)
	./$(BUILD)/test_dataflowbench

test_dataflow_cycle:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		tests/test_dataflow_cycle.cpp \
		src/Dataflow.cpp \
		src/PEArray.cpp \
		src/PE.cpp \
		-o $(BUILD)/test_dataflow_cycle $(LIBS)
	./$(BUILD)/test_dataflow_cycle

mnist_conv:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		src/main.cpp \
		src/Testbench.cpp \
		src/PE.cpp \
		src/PEArray.cpp \
		src/Dataflow.cpp \
		-o $(BUILD)/mnist_conv -L$(SYSTEMC)/lib-linux64 -lsystemc -pthread
	./$(BUILD)/mnist_conv workloads/mnist 0

clean:
	rm -rf $(BUILD)

.PHONY: clean