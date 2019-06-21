/**
 *  StringTree.cpp
 *  ONScripter-RU
 *
 *  A hierarchical tree-like structure.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Entities/StringTree.hpp"
#include "Engine/Core/ONScripter.hpp"

std::string StringTree::getValue(std::deque<std::string> &ss) {
	if (!ss.empty()) {
		std::string val = ss.front();
		ss.pop_front();
		auto entry = branches.find(val);
		if (entry == branches.end()) {
			throw std::runtime_error("Tried to get from nonexistent key " + val);
		}
		StringTree &subtree = entry->second;
		return subtree.getValue(ss);
	}
	return value;
}

int StringTree::setValue(std::deque<std::string> &ss, std::string &value) { //unwilling to use the last element of a deque
	int autoNum = -1;
	if (!ss.empty()) {
		std::string val(std::move(ss.front()));
		ss.pop_front();
		if (val == "auto") {
			autoNum = static_cast<int>(branches.size());
			val     = std::to_string(autoNum);
			(*this)[val].setValue(ss, value);
		} else {
			autoNum = (*this)[val].setValue(ss, value);
		}
	} else {
		this->value = value;
	}
	return autoNum;
}

void StringTree::prune(std::deque<std::string> &ss) {
	if (ss.size() > 1) {
		std::string val(std::move(ss.front()));
		ss.pop_front();
		(*this)[val].prune(ss);
	} else if (ss.size() == 1) {
		std::string val(std::move(ss.front()));
		ss.pop_front();
		branches.erase(val);
	}
}

void StringTree::clear() {
	branches.clear();
	insertionOrder.clear();
	value = "";
}

void StringTree::accept(const std::shared_ptr<IStringTreeVisitor> &visitor) {
	visitor->visit(*this);
}

void StringTree::StringTreeExecuter::visit(StringTree &tree) {
	realVisit(tree);
}

bool StringTree::StringTreeExecuter::realVisit(StringTree &tree) {
	int res{0};

	// It actually doesn't matter if we are reexecuting more or this is the last run (we have no more commands)
	// We should unset our flag in case it was set by user command and later we didn't call getparam
	// Should be noted, that this model will not work in case of paramless user cmd calling a user cmd with params
	ons.inVariableQueueSubroutine = false;
	for (auto &pair : tree.branches) {
		if (realVisit(pair.second)) {
			return true;
		}
	}
	if (tree.has(0)) {
		// This node represents a single command.
		res = ons.executeSingleCommandFromTreeNode(tree);

		//We should erase the entry we have just executed, so that it doesn't get called again
		tree.clear();

		if (res != ONScripter::RET_NO_READ) {
			// We need to return to the main script, executeSingleCommandFromTreeNode entered user command
			// We will be back soon and reexecute this command (thanks to executeSingleCommandFromTreeNode actions)
			return true;
		}
	}
	return false;
}

void StringTree::StringTreePrinter::visit(StringTree &tree) {
	realVisit(tree, 0);
}

void StringTree::StringTreePrinter::realVisit(StringTree &tree, int indent) {
	std::string s(indent, ' ');
	if (tree.value != "") {
		std::cerr << tree.value << std::endl;
	}
	if (tree.insertionOrder.empty())
		return;
	std::cerr << "{" << std::endl;
	for (std::string &c : tree.insertionOrder) {
		std::cerr << s << c << ": ";
		realVisit(tree.branches[c], indent + 2);
	}
	std::cerr << s << "}" << std::endl;
}
