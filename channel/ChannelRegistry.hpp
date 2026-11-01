#ifndef CHANNEL_REGISTRY_HPP
#define CHANNEL_REGISTRY_HPP

#include "Channel.hpp"
#include <map>
#include <memory>

class ChannelRegistry
{
	private:
		std::map<std::string, std::unique_ptr<Channel>> _channels;

	public:
		ChannelRegistry();
		~ChannelRegistry();
		ChannelRegistry(const ChannelRegistry&) = delete;
		ChannelRegistry& operator=(const ChannelRegistry&) = delete;
		Channel* findChannel(const std::string& name);
		Channel* findOrCreate(const std::string& name);
		void removeChannel(const std::string& name);
		void removeClientFromAllChannels(int fd);
};

#endif
