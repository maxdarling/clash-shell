#include <vector>
#include <string>

/** 
 * This class implements a simple shell called "clash", modeled after bash. 
 * 
 * A single 'run' method is provided, which invokes a clash instance on the 
 * given argument array and writes output to standard out.   
 * 
 *
 * Usage: 
 * - If no arguments are passed, clash will read commands from standard input. 
 *   A '%' prompt is used for terminals.  
 *
 * - If the first argument is "-c", then the 2nd argument must be a shell script, 
 *   which clash will execute and then exit. 
 *
 * - Otherwise, the first argument must be the name of a file, from which clash
 *   will execute commands and exit once it reaches the end of the file.    
 */ 

class Clash {
  public: 
   static void run(std::vector<std::string> args); 
};