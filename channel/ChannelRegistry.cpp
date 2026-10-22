#include "ChannelRegistry.hpp"
#include <memory>

ChannelRegistry::ChannelRegistry() {}

ChannelRegistry::~ChannelRegistry() {
	while (!_channels.empty()) {
		delete _channels.begin()->second;
		_channels.erase(_channels.begin());
	}
}

Channel* ChannelRegistry::findChannel(const std::string& name)
{
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	if (it == _channels.end())
		return NULL;
	return it->second;
}

Channel* ChannelRegistry::findOrCreate(const std::string& name)
{
	Channel* channel = findChannel(name);
	if(channel != NULL)
		return channel;
	std::auto_ptr<Channel> ownedChannel(new Channel(name));
	channel = ownedChannel.get();
	_channels[name] = channel;
	ownedChannel.release();
	return channel;
}

void ChannelRegistry::removeChannel(const std::string& name)
{
	std::map<std::string, Channel*>::iterator it = _channels.find(name);
	if (it == _channels.end())
		return;
	delete it->second;
	_channels.erase(it);
}

void ChannelRegistry::removeClientFromAllChannels(int fd)
{
	std::map<std::string, Channel*>::iterator it = _channels.begin();
	while (it != _channels.end()) {
		it->second->removeMember(fd);
		if (it->second->getMemberCount() == 0) {
			delete it->second;
			_channels.erase(it++);
		} else {
			++it;
		}
	}
}
