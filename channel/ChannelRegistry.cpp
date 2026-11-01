#include "ChannelRegistry.hpp"

ChannelRegistry::ChannelRegistry() = default;

ChannelRegistry::~ChannelRegistry() = default;

Channel* ChannelRegistry::findChannel(const std::string& name)
{
	const auto it = _channels.find(name);
	return it == _channels.end() ? nullptr : it->second.get();
}

Channel* ChannelRegistry::findOrCreate(const std::string& name)
{
	if (Channel* channel = findChannel(name))
		return channel;
	return _channels.emplace(name, std::make_unique<Channel>(name)).first->second.get();
}

void ChannelRegistry::removeChannel(const std::string& name)
{
	_channels.erase(name);
}

void ChannelRegistry::removeClientFromAllChannels(int fd)
{
	for (auto it = _channels.begin(); it != _channels.end();) {
		it->second->removeMember(fd);
		if (it->second->getMemberCount() == 0)
			it = _channels.erase(it);
		else
			++it;
	}
}
