SYSCONF_LINK = g++
CPPFLAGS     = -Wall -Wextra -Weffc++ -pedantic -std=c++11 -O3 -I/opt/homebrew/include
LDFLAGS      = -L/opt/homebrew/lib
LIBS         = -lSDL2 -lSDL2_ttf -lSDL2_mixer -lGLEW -framework OpenGL -lm

DESTDIR = ./
TARGET  = main

OBJECTS := $(patsubst %.cpp,%.o,$(wildcard *.cpp))
DEPS    := $(OBJECTS:.o=.d)

all: $(DESTDIR)$(TARGET)

$(DESTDIR)$(TARGET): $(OBJECTS)
	$(SYSCONF_LINK) -Wall $(LDFLAGS) -o $(DESTDIR)$(TARGET) $(OBJECTS) $(LIBS)

# -MMD -MP makes the compiler emit a .d file per .o listing every header it
# touched. We include those below so editing a header re-triggers the right .o
# rebuilds and we never hit the stale-layout crashes again.
$(OBJECTS): %.o: %.cpp
	$(SYSCONF_LINK) -Wall $(CPPFLAGS) -MMD -MP -c $(CFLAGS) $< -o $@

-include $(DEPS)

clean:
	-rm -f $(OBJECTS)
	-rm -f $(DEPS)
	-rm -f $(TARGET)
