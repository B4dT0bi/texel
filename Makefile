SRC	= bitBoard.cpp book.cpp computerPlayer.cpp enginecontrol.cpp evaluate.cpp \
	  game.cpp history.cpp humanPlayer.cpp killerTable.cpp kpkTable.cpp \
	  krkpTable.cpp move.cpp moveGen.cpp parameters.cpp piece.cpp position.cpp \
	  random.cpp search.cpp texel.cpp textio.cpp transpositionTable.cpp \
	  treeLogger.cpp tuigame.cpp uciprotocol.cpp util.cpp
OBJS32	= $(patsubst %.cpp,obj32/%.o,$(SRC))
OBJS64	= $(patsubst %.cpp,obj64/%.o,$(SRC))

CXX = g++

CXXFLAGS	= -O3 -Wall -std=c++0x -pthread
LDFLAGS		= -pthread

all	: texel32 texel64

dist	: texel.zip

texel.zip: FORCE texel32 texel64
	(VER=$$(echo -e 'uci\nquit' | ./texel32 | grep 'id name' | awk '{print $$4}' | tr -d .) ; \
	 rm -f texel$${VER}.zip ; \
	 zip -r texel$${VER}.zip Makefile src texel{32,64}{,.exe})

$(OBJS32) : obj32/%.o : src/%.cpp
	-mkdir -p obj32
	$(CXX) $(CXXFLAGS) -m32 -c -o $@ $<

texel32  : $(OBJS32) Makefile
	$(CXX) $(LDFLAGS) -m32 -o $@ $(OBJS32)


$(OBJS64) : obj64/%.o : src/%.cpp
	-mkdir -p obj64
	$(CXX) $(CXXFLAGS) -m64 -c -o $@ $<

texel64	 : $(OBJS64) Makefile
	$(CXX) $(LDFLAGS) -m64 -o $@ $(OBJS64)


clean 	: 
	rm -rf $(OBJS) *~ obj32 obj64

.PHONY	: clean dist FORCE
