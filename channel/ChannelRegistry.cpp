#include "ChannelRegistry.hpp"
#include <cctype>

std::string ChannelRegistry::normalizeName(const std::string& name) {
	std::string folded;
	folded.reserve(name.size());
	for (unsigned char ch : name) {
		folded.push_back(static_cast<char>(std::tolower(ch)));
	}
	return folded;
}

ChannelRegistry::ChannelRegistry() = default;

ChannelRegistry::~ChannelRegistry() = default;

Channel* ChannelRegistry::findChannel(const std::string& name)
{
	const auto it = _channels.find(normalizeName(name));
	return it == _channels.end() ? nullptr : it->second.get();
}

Channel* ChannelRegistry::findOrCreate(const std::string& name)
{
	const std::string normalized = normalizeName(name);
	if (Channel* channel = findChannel(name))
		return channel;
	return _channels.emplace(normalized, std::make_unique<Channel>(name)).first->second.get();
}

void ChannelRegistry::removeChannel(const std::string& name)
{
	_channels.erase(normalizeName(name));
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

std::vector<std::string> ChannelRegistry::listChannelNames() const
{
	std::vector<std::string> names;
	for (const auto& entry : _channels) {
		if (entry.second)
			names.push_back(entry.second->getName());
	}
	return names;
}
