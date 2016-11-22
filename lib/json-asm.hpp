#ifndef JSON_ASM
#define JSON_ASM

namespace json_asm {
	void *execute(Command c, rapidjson::Document *d);
	std::vector<Command> tokenize(std::string *tape);
}

#endif
