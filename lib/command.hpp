#ifndef COMMAND

#define COMMAND

#include <string>
#include <sstream>
#include <vector>

class Command {
public:
	std::string op;
	std::vector<std::string> arguments;

	Command() {}
	~Command() {}
};

// Tokenize a string containing commands into command objects
std::vector<Command> tokenize(std::string *tape);
#endif
