#ifndef CHANNEL_REGISTRY_HPP
#define CHANNEL_REGISTRY_HPP

#include "Channel.hpp"
#include <map>

class ChannelRegistry
{
	private:
		std::map<std::string, Channel*> _channels;
		ChannelRegistry(const ChannelRegistry& other);
		ChannelRegistry& operator=(const ChannelRegistry& other);

	public:
		ChannelRegistry();
		~ChannelRegistry();
		Channel* findChannel(const std::string& name);
		Channel* findOrCreate(const std::string& name);
		void removeChannel(const std::string& name);
		void removeClientFromAllChannels(int fd);
};

#endif
