CC=g++ -O3 #-static-libstdc++
BIN_NAME=pdbm
OBJ_DIR=obj
BIN_DIR=bin

$(BIN_DIR)/$(BIN_NAME): $(OBJ_DIR)/main.o
	@mkdir -p $(BIN_DIR)
	${CC} -o $@ $^ -pthread -lstdc++fs -L${BOOST_ROOT}/lib -lboost_program_options

$(OBJ_DIR)/%.o: ./%.cpp
	@mkdir -p $(OBJ_DIR)
	${CC} -o $@ $< -c -std=c++17 -DNDEBUG -Wall -I${BOOST_ROOT}/include

clean:
	rm -f $(BIN_DIR)/$(BIN_NAME) $(OBJ_DIR)/*.o

setup:
	sudo cp $(BIN_DIR)/$(BIN_NAME) /usr/bin
