NAME := ircserv
BUILD_DIR := build

CXX ?= c++
CPPFLAGS ?=
CXXFLAGS ?= -Wall -Wextra -Werror -std=c++20
LDFLAGS ?=
LDLIBS ?=

SOURCES := \
	Server/Server.cpp \
	main.cpp \
	client/authentification.cpp \
	commands/registration.cpp \
	commands/parser.cpp \
	commands/messaging.cpp \
	commands/commands.cpp \
	client/client.cpp \
	commands/channel/helpers.cpp \
	channel/Channel.cpp \
	channel/ChannelRegistry.cpp \
	commands/channel/join.cpp \
	commands/channel/part.cpp \
	commands/channel/topic.cpp \
	commands/channel/invite.cpp \
	commands/channel/kick.cpp \
	commands/channel/mode.cpp

OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
LEGACY_OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all test clean fclean re

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

test: $(NAME)
	PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(LEGACY_OBJECTS)

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

-include $(DEPFILES)
