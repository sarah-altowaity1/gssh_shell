#ifndef INPUT_PARSING
#define INPUT_PARSING

char* read_input();
char** tokenize(char*);
char** tokenizePipes(char*, int*);
void remove_special_chars(char*);

#endif