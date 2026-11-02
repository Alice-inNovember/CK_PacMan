.DEFAULT_GOAL := All

CXX = clang++
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -IScripts
BUILD_DIR := Build
TEMP_DIR := Temp

ifeq ($(OS),Windows_NT)
EXE := .exe
endif

SFML_CFLAGS := $(shell pkg-config --cflags sfml-graphics 2>/dev/null)
SFML_LIBS := $(shell pkg-config --libs sfml-graphics 2>/dev/null)
MAP_OBJECTS := $(TEMP_DIR)/Map.o $(TEMP_DIR)/MapLoader.o $(TEMP_DIR)/WallVisual.o
AI_OBJECTS := \
	$(TEMP_DIR)/GhostController.o \
	$(TEMP_DIR)/PatrolBehavior.o $(TEMP_DIR)/NavigationGraph.o $(TEMP_DIR)/AStar.o \
	$(TEMP_DIR)/GhostState.o $(TEMP_DIR)/Ghost.o $(TEMP_DIR)/GhostRed.o \
	$(TEMP_DIR)/GhostGreen.o $(TEMP_DIR)/GhostBlue.o
PLAYER_OBJECTS := $(TEMP_DIR)/Player.o
SYSTEM_OBJECTS := \
	$(TEMP_DIR)/RoundState.o $(TEMP_DIR)/GhostActor.o $(TEMP_DIR)/Mover.o \
	$(TEMP_DIR)/Game.o
CORE_OBJECTS := $(MAP_OBJECTS) $(AI_OBJECTS) $(PLAYER_OBJECTS) $(SYSTEM_OBJECTS)
RENDER_OBJECTS := \
	$(TEMP_DIR)/GamePresentation.o $(TEMP_DIR)/HudRenderer.o $(TEMP_DIR)/TextureStore.o \
	$(TEMP_DIR)/SpriteAnimator.o $(TEMP_DIR)/AssetRenderer.o
GAME_OBJECTS := $(CORE_OBJECTS) $(TEMP_DIR)/Debug.o $(RENDER_OBJECTS) $(TEMP_DIR)/Main.o
EDITOR_OBJECTS := $(MAP_OBJECTS) $(TEMP_DIR)/TextureStore.o $(TEMP_DIR)/EditorDocument.o $(TEMP_DIR)/MapEditorMain.o
OBJECTS := $(sort $(GAME_OBJECTS) $(EDITOR_OBJECTS))
DEPS := $(OBJECTS:.o=.d)
GAME_TARGET := $(BUILD_DIR)/PacMan$(EXE)
EDITOR_TARGET := $(BUILD_DIR)/MapEditor$(EXE)

.PHONY: All Clean Fclean Re MapEditor

All: $(GAME_TARGET) $(EDITOR_TARGET)

MapEditor: $(EDITOR_TARGET)

$(TEMP_DIR)/%.o: Scripts/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(MAP_OBJECTS): $(TEMP_DIR)/%.o: Scripts/Map/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(AI_OBJECTS): $(TEMP_DIR)/%.o: Scripts/AI/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(PLAYER_OBJECTS): $(TEMP_DIR)/%.o: Scripts/Player/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(SYSTEM_OBJECTS) $(TEMP_DIR)/Debug.o: $(TEMP_DIR)/%.o: Scripts/System/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(RENDER_OBJECTS): $(TEMP_DIR)/%.o: Scripts/Rendering/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(TEMP_DIR)/%.o: Scripts/MapEditor/%.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(TEMP_DIR)/MapEditorMain.o: Scripts/MapEditor/Main.cpp Makefile
	mkdir -p "$(TEMP_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c "$<" -o "$@"

$(TEMP_DIR)/Debug.o $(RENDER_OBJECTS) $(TEMP_DIR)/Main.o $(TEMP_DIR)/MapEditorMain.o: CPPFLAGS += $(SFML_CFLAGS)

$(GAME_TARGET): $(GAME_OBJECTS)
	mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(SFML_LIBS) $(LDLIBS) -o "$@"

$(EDITOR_TARGET): $(EDITOR_OBJECTS)
	mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(SFML_LIBS) $(LDLIBS) -o "$@"

Clean:
	$(RM) $(OBJECTS) $(DEPS)

Fclean: Clean
	$(RM) "$(GAME_TARGET)" "$(EDITOR_TARGET)"

Re:
	$(MAKE) Fclean
	$(MAKE) All

-include $(DEPS)
