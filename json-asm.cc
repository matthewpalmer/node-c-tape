#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "rapidjson/pointer.h"
#include "deps/rapidjson/include/rapidjson/document.h"
#include "command.hpp"
#include "json-asm.hpp"

using namespace rapidjson;

namespace json_asm {

  /**
   * Execute
   */
  Value *load(Command command, rapidjson::Document *d) {
    std::string operand = command.arguments[0];

    // Boolean literals
    if (operand == "true" || operand == "false") {
      return new Value(operand == "true");
    }

    // String literals
    if (operand[0] == '\'' || operand[0] == '"') {
      const char *sub = operand.substr(1, operand.length() - 2).c_str();
      return new Value(sub, d->GetAllocator());
    }

    // Number literals
    try { // stoi and stod throw
      std::size_t consumed = 0;

      int intValue = std::stoi(operand, &consumed);
      if (consumed == operand.length()) {
        return new Value(intValue);
      }

      consumed = 0;
      double doubleValue = std::stod(operand, &consumed);
      if (consumed == operand.length()) {
        return new Value(doubleValue);
      }
    } catch (std::invalid_argument) {}

    // Dereference pointers
    Value *v = Pointer(operand.c_str()).Get(*d);
    return v;
  }

  void *store(Command command, rapidjson::Document *d) {
    Command c;
    c.op = "load";
    c.arguments.push_back(command.arguments[1]);
    Value *rvalue = load(c, d);

    const char *cstr = command.arguments[0].c_str();
    Pointer(cstr).Set(*d, *rvalue);

    return NULL;
  }

  void *add(Command command, rapidjson::Document *d) {
    Command loadOp1;
    loadOp1.op = "load";
    loadOp1.arguments.push_back(command.arguments[1]);

    Command loadOp2;
    loadOp2.op = "load";
    loadOp2.arguments.push_back(command.arguments[2]);

    Value *operand1 = static_cast<Value *>(execute(loadOp1, d));
    Value *operand2 = static_cast<Value *>(execute(loadOp2, d));

    if (operand1->GetType() != operand2->GetType()) return NULL;

    Command storeCommand;
    storeCommand.op = "store";
    storeCommand.arguments.push_back(command.arguments[0]);

    if (operand1->IsInt()) {
      int result = operand1->GetInt() + operand2->GetInt();
      storeCommand.arguments.push_back(std::to_string(result));
    } else if (operand1->IsDouble()) {
      double result = operand1->GetDouble() + operand2->GetDouble();
      storeCommand.arguments.push_back(std::to_string(result));
    }

    execute(storeCommand, d);

    return NULL;
  }

  void *sort(Command c, rapidjson::Document *d) {
    // TODO
    return NULL;
  }

  void *execute(Command c, rapidjson::Document *d) {
    // Trying to store a map of op -> function pointer is a total pain, 
    // so we just do our function calls manually.
    if (c.op == "store") {
      return store(c, d);
    } else if (c.op == "add") {
      return add(c, d);
    } else if (c.op == "load") {
      return load(c, d);
    } else if (c.op == "sort") {
      return sort(c, d);
    }

    return NULL;
  }


  /**
   * Tokenize
   */

  // http://stackoverflow.com/a/236803
  static void split(const std::string &s, char delim, std::vector<std::string> &elems) {
      std::stringstream ss;
      ss.str(s);
      std::string item;
      while (std::getline(ss, item, delim)) {
          elems.push_back(item);
      }
  }

  static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
  }

  // add:results[0]:weight:results[0].weight:2
  std::vector<Command> tokenize(std::string *tape) {
    char fieldDelimiter = ':';
    char commandDelimiter = '\n';

    std::vector<Command> commands;
    std::vector<std::string> lines = split(*tape, commandDelimiter);

    for (auto const& line: lines) {
      std::string op;
      std::vector<std::string> arguments;

      bool isFirst = true;
      std::string acc = "";
      char matchingQuote = 0;

      for (unsigned int i = 0; i < line.length(); i++) {
        if (line[i] == '"' || line[i] == '\'') {
          acc += line[i];

          if (!matchingQuote) {
            matchingQuote = line[i];
          } else if (matchingQuote == line[i]) {
            matchingQuote = 0;
          }
        } else if (line[i] == fieldDelimiter && !matchingQuote) {
          if (isFirst) {
            op = acc;
            isFirst = false;
          } else {
            arguments.push_back(acc);  
          }
          
          acc = "";
        } else {
          acc += line[i];
        }
      }

      if (acc.length() > 0) {
        arguments.push_back(acc);
      }

      Command c;
      c.op = op;
      c.arguments = arguments;

      commands.push_back(c);
    }

    return commands;
  }
}
