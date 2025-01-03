#ifndef YAML_UTILS_H
#define YAML_UTILS_H


#include <ut_kvp_profile.h>
#include <ut_kvp.h>
#include <vector>
#include <string>
#include <iostream>

#define UNUSED(x) (void)(x) // Define the UNUSED macro


// dummy yaml class to make api direct replaceble in client

class YAML{
	public :
	class BadFile : public std::exception{
	
	};
	class BadConversion : public std::exception{
	
	};		
	class ParserException : public std::exception{
	
	};
	class Node{
		
		public:
		static ut_kvp_instance_t * p_instance;

	};
	static Node LoadFile(std::string filename){
 		YAML::Node rtn;
        	YAML::Node::p_instance = ut_kvp_createInstance();
        	int status = ut_kvp_open( YAML::Node::p_instance, const_cast<char *>(filename.c_str()));
        	if(status != 0){
                	std::cerr << "Failed to create parser method for " << std::endl;
        	}	
        	return rtn;
	}
	static void CloseFile( Node l_v){
        	if(YAML::Node::p_instance != nullptr ){
                	ut_kvp_close( YAML::Node::p_instance);
                	return ;
        	}
  	}
};


// Function to get the count of elements in a list at the given path
 int get_list_Count_from_profile( YAML::Node d_conf, const std::string& path);
//
// // Function to get an integer value from a YAML node based on a path
 int get_integer_value_from_profile( YAML::Node d_conf, const std::string& path);
//
// // Function to extract a string value based on the path
 std::string get_string_value_from_profile( YAML::Node d_conf, const std::string& path);
//
// // Function to extract a boolean value based on the path
 bool get_boolean_value_from_profile(YAML::Node d_conf, const std::string& path);

#endif
