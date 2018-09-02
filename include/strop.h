#ifndef _strop_h_
#define _strop_h_

void str_trim_crlf(char* str);
void str_spilt(const char* str, char* left, char* right, char c);
bool str_all_space(const char* str);
void str_upper(char* str);
unsigned int str_octal_to_unit(const char* str);

#endif