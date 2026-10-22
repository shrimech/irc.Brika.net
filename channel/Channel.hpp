#pragma once
#include <string>
#include <map>
#include <set>
#include <cstddef>
#include "../client/client.hpp"



class Channel
{
	private:
		std::string _name;
		std::string _topic;
		std::string _key;
		int _userLimit;
		std::map<int, Client*> _members;
		std::set<int> _operators;
		std::set<int> _invited;
		bool _inviteOnly;
		bool _topicRestricted;
		bool _hasKey;
		bool _hasUserLimit;
		Channel(const Channel& other);
		Channel& operator=(const Channel& other);

	public:
		Channel(const std::string& name);
		~Channel();

		std::string getName() const;

		void addMember(int fd, Client* client);
		void removeMember(int fd);
		bool isMember(int fd) const;
		size_t getMemberCount() const;

		void addOperator(int fd);
		void removeOperator(int fd);
		bool isOperator(int fd) const;

		void addInvite(int fd);
		bool isInvited(int fd) const;
		void removeInvite(int fd);

		std::string getTopic() const;
		void setTopic(const std::string& topic);
		bool hasTopic() const;

		bool isInviteOnly() const;
		void setInvitedOnly(bool value);

		bool isTopicRestricted() const;
		void setTopicRestricted(bool value);

		bool hasKey() const;
		void setKey(const std::string& key);
		void removeKey();
		std::string getKey() const;

		bool hasUserLimit() const;
		void setUserLimit(int limit);
		void removeUserLimit();
		int getUserLimit() const;

		std::string getMembersName() const;

		void broadCast(const std::string& msg, int excludeFD = -1) const;

};
