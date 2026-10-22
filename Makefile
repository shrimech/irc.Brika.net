NAME := ircserv
BUILD_DIR := build

CXX ?= c++
CPPFLAGS ?=
CXXFLAGS ?= -Wall -Wextra -Werror -std=c++98
LDFLAGS ?=
LDLIBS ?=

SOURCES := \
	Server/Server.cpp \
	main.cpp \
	client/authentification.cpp \
	client/commands.cpp \
	client/client.cpp \
	channel/helpers.cpp \
	channel/Channel.cpp \
	channel/ChannelRegistry.cpp \
	channel/join.cpp \
	channel/topic.cpp \
	channel/invite.cpp \
	channel/kick.cpp \
	channel/mode.cpp

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
