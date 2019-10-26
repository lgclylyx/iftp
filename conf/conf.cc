#include "common.h"
#include "strop.h"

bool pasv_enable = true;
bool port_enable = true;
int listen_port = 21;
int max_clients = 2000;
int max_per_ip = 50;
int accept_timeout = 60;
int connect_timeout = 60;
int session_timeout = 300;
int data_connection_timeout = 300;
int local_umask = 077;
int upload_max_rate = 0;
int download_max_rate = 0;
const char* listen_address = NULL;

static struct parseconf_bool_setting{
	const char* p_setting_name;
	bool *p_variable;
}parseconf_bool_array[] = {
	{"pasv_enable",&pasv_enable},
	{"port_enable",&port_enable},
	{NULL,NULL}
};

static struct parseconf_unit_setting{
	const char* p_setting_name;
	int *p_variable;
}parseconf_unit_array[] = {
	{"listen_port" ,&listen_port},
	{"max_clients" ,&max_clients},
	{"max_per_ip" ,&max_per_ip},
	{"accept_timeout" ,&accept_timeout},
	{"connect_timeout" ,&connect_timeout},
	{"session_timeout" , &session_timeout},
	{"data_connection_timeout" ,&data_connection_timeout},
	{"local_umask" , &local_umask},
	{"upload_max_rate" , &upload_max_rate},
	{"download_max_rate" , &download_max_rate},
	{NULL,NULL}
};

static struct parseconf_str_setting{
	const char* p_setting_name;
	const char** p_variable;
}parseconf_str_array[]={
	{"listen_address",&listen_address},
	{NULL,NULL}
};


void parseconf_load_setting(const char* setting);

void parseconf_load_file(const char* path){
	std::ifstream in(path);
	if(!in.is_open())
		return;
	std::string setting_line;
	while(getline(in,setting_line)){
		if(setting_line.back() == '\r')
			setting_line[setting_line.size()-1] = '\0';
		if(setting_line.empty() 
		   || setting_line[0] == '#'
		   || str_all_space(setting_line.c_str()))
			continue;
		parseconf_load_setting(setting_line.c_str());
		setting_line.clear();
	}
}

void parseconf_load_setting(const char* setting){
	while(isspace(*setting))
		setting++;

	char key[128] = {0};
	char value[128] = {0};
	str_spilt(setting,key,value,'=');
	if(strlen(value) == 0)
		return;

	{
		const struct parseconf_str_setting* p_str_setting = parseconf_str_array;
		while(p_str_setting->p_setting_name != NULL){
			if(strcmp(key,p_str_setting->p_setting_name) == 0){
				const char **p_cur_setting = p_str_setting->p_variable;
				if(*p_cur_setting)
					free(const_cast<char*>(*p_cur_setting));
				*p_cur_setting = strdup(value);
				return; 
			}
			p_str_setting++;
		}
	}

	{
		const struct parseconf_bool_setting* p_bool_setting = parseconf_bool_array;
		while(p_bool_setting->p_setting_name != NULL){
			if(strcmp(key,p_bool_setting->p_setting_name) == 0){
				str_upper(value);
				if(strcmp(value,"TRUE") == 0)
					*(p_bool_setting->p_variable) = true;
				else if(strcmp(value,"FALSE") == 0)
					*(p_bool_setting->p_variable) = false;
				return; 
			}
			p_bool_setting++;
		}
	}

	{
		const struct parseconf_unit_setting* p_unit_setting = parseconf_unit_array;
		while(p_unit_setting->p_setting_name != NULL){
			if(strcmp(key,p_unit_setting->p_setting_name) == 0){
				if(value[0] == '0')
					*(p_unit_setting->p_variable) = str_octal_to_unit(value);
				else
					*(p_unit_setting->p_variable) = atoi(value);

				return; 
			}
			p_unit_setting++;
		}
	}
}
