#pragma once

#include <string>

class Interpreter
{
public:
  
  static void Initilize();

  static void InterpretCANFile(const char* rawDataFile);
  static void InterpretCANFile(std::string rawDataFile);

  static void Shutdown();

};
