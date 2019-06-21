/**
 *  StringTree.hpp
 *  ONScripter-RU
 *
 *  A hierarchical tree-like structure.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <unordered_map>
#include <string>
#include <deque>
#include <vector>

class StringTree {
public:
	struct IStringTreeVisitor {
		virtual void visit(StringTree &tree) = 0;
		virtual ~IStringTreeVisitor() = default;
	};

	struct StringTreeExecuter : public IStringTreeVisitor {
		void visit(StringTree &tree) override;
		bool realVisit(StringTree &tree);
	};

	struct StringTreePrinter : public IStringTreeVisitor {
		void visit(StringTree &tree) override;
		void realVisit(StringTree &tree, int indent);
	};

	// API is simply too restrictive with these private, you should have seen that hell
	std::string value;
	std::unordered_map<std::string, cmp::any<StringTree>> branches;
	std::vector<std::string> insertionOrder;

	void accept(const std::shared_ptr<IStringTreeVisitor> &visitor);
	std::string getValue(std::deque<std::string> &ss);
	int setValue(std::deque<std::string> &ss, std::string &value);
	void prune(std::deque<std::string> &ss);
	void clear();
	bool has(const std::string &key) {
		return branches.count(key);
	}
	bool has(long key) {
		return branches.count(std::to_string(key));
	}
	StringTree &operator[](const std::string &&key) {
		bool existed = this->branches.count(key);
		auto &ret    = branches[key];
		if (!existed)
			this->insertionOrder.push_back(key);
		return ret;
	}
	StringTree &operator[](const std::string &key) {
		bool existed = this->branches.count(key);
		auto &ret    = branches[key];
		if (!existed)
			this->insertionOrder.push_back(key);
		return ret;
	}
	StringTree &operator[](const long key) {
		return operator[](std::to_string(key));
	}
	StringTree &getById(long key) {
		return (*this)[insertionOrder[key]];
	}
};
