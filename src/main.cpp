#include "git.h"
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  if (argc < 2) {
    std::cerr << "No command provided.\n";
    return EXIT_FAILURE;
  }

  std::string command = argv[1];

  if (command == "init") {
    git::init();
  } 
  else if (command == "cat-file") {
    git::cat_file(argv[3]);
  } 
  else if(command == "hash-object"){
    if(argc<=3){
      std::cerr << "Invalid Arguments"<<std::endl;
      return EXIT_FAILURE;
    }
      std::string mode = argv[2];
      if(mode!= "-w"){
        std::cerr << "Invalid mode for hash-object, expected '-w'"<<std::endl;
        return EXIT_FAILURE;
      }
      git::hash_object(argv[3]);
  }
  else if(command == "ls-tree"){
    if(argc==4) git::ls_tree_name_only(argv[3]);
    else git::ls_tree(argv[2]);
  }
  else if (command == "write-tree") {
      std::string sha = writeTree(fs::current_path());
      
  }
  else {
    std::cerr << "Unknown command " << command << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
