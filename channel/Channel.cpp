#include "Channel.hpp"

Channel::Channel(const std::string& name): _name(name), _userLimit(0), _inviteOnly(false),
			_topicRestricted(true), _hasKey(false), _hasUserLimit(false) {}
Channel::~Channel() {}

std::string Channel::getName() const {
	return _name;
}

void Channel::addMember(int fd, Client* client) {
	_members[fd] = client;
}
void Channel::removeMember(int fd) {
	_members.erase(fd);
	_operators.erase(fd);
	_invited.erase(fd);
}
bool Channel::isMember(int fd) const {
	return _members.find(fd) != _members.end();
}
size_t Channel::getMemberCount() const {
	return _members.size();
}
void Channel::addOperator(int fd) {
	_operators.insert(fd);
}
void Channel::removeOperator(int fd) {
	_operators.erase(fd);
}
bool Channel::isOperator(int fd) const {
	return _operators.find(fd) != _operators.end();
}


void Channel::addInvite(int fd){
	_invited.insert(fd);
}
bool Channel::isInvited(int fd) const {
	return _invited.find(fd) != _invited.end();
}
void Channel::removeInvite(int fd) {
	_invited.erase(fd);
}


std::string Channel::getTopic() const {
	return _topic;
}
void Channel::setTopic(const std::string& topic) {
	_topic = topic;
}
bool Channel::hasTopic() const {
	return !_topic.empty();
}


bool Channel::isInviteOnly() const {
	return _inviteOnly;
}
void Channel::setInvitedOnly(bool value) {
	_inviteOnly = value;
}


bool Channel::isTopicRestricted() const {
	return _topicRestricted;
}
void Channel::setTopicRestricted(bool value) {
	_topicRestricted = value;
}


bool Channel::hasKey() const {
	return _hasKey;
}
void Channel::setKey(const std::string& key) {
	_key = key;
	_hasKey = true;
}
void Channel::removeKey() {
	_key = "";
	_hasKey = false;
}
std::string Channel::getKey() const {
	return _key;
}


bool Channel::hasUserLimit() const {
	return _hasUserLimit;
}
void Channel::setUserLimit(int limit) {
	_hasUserLimit = true;
	_userLimit = limit;
}
void Channel::removeUserLimit() {
	_hasUserLimit = false;
}
int Channel::getUserLimit() const {
	return _userLimit;
}

std::string Channel::getMembersName() const
{
	std::string names;
	std::map<int, Client*>::const_iterator it = _members.begin();
	for (; it != _members.end(); it++)
	{
		if (isOperator(it->first))
			names += "@";
		names += it->second->getNickname();
		if (it != --(_members.end()))
			names += " ";
	}
	return names;
}

void Channel::broadCast(const std::string& msg, int excludeFD) const
{
	std::map<int, Client*>::const_iterator it = _members.begin();
	for (; it != _members.end(); it++)
	{
		if (it->first != excludeFD)
			it->second->sendMessage(msg);
	}
}
