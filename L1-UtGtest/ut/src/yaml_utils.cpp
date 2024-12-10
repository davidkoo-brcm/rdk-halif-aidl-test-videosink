#include "yaml_utils.h"

// Helper function to split the path into segments
std::vector<std::string> splitPath(const std::string& path, char delimiter); 
std::vector<std::string> splitPath(const std::string& path, char delimiter) {
    std::vector<std::string> segments;
    size_t start = 0;
    size_t end = path.find(delimiter);

    while (end != std::string::npos) {
        segments.push_back(path.substr(start, end - start));
        start = end + 1;
        end = path.find(delimiter, start);
    }

    segments.push_back(path.substr(start));
    return segments;
}


ut_kvp_instance_t * YAML::Node::p_instance = nullptr;

// Function to get the count of elements in a list at the given path
int get_list_Count_from_profile( YAML::Node d_conf, const std::string& path){

	//UNUSED(d_conf);
//split path
	int count = 0;
	std::vector<std::string> segments = splitPath(path, '.');

	if (segments.empty() || segments[0] != "components") {
		std::cerr << "Invalid path or incorrect format." << std::endl;
		return -1;		 
	}

	int total_componentes = ut_kvp_getListCount(YAML::Node::p_instance ,segments[0].c_str());

	if(total_componentes == 0){
		std::cerr << "components node not found or not a list." << std::endl;
		return -1;
	}

	bool found = false;
	int i = 0;
	char result_kvp[UT_KVP_MAX_ELEMENT_SIZE];
		//we have node lets find propername
	for(; i < total_componentes;i++){
	
	std::string to_find_key = segments[0] +"/"+ std::to_string(i) + "/" + "name";//segments[1];
		if(0 == ut_kvp_getStringField(YAML::Node::p_instance, to_find_key.c_str(), result_kvp, UT_KVP_MAX_ELEMENT_SIZE)){
		
			if(segments[1].compare(result_kvp) == 0){
				found = true;
				break;		//check the count no
			}
		}
	}
	if(!found){
		std::cerr << "Component with name " << segments[1] << " not found." << std::endl;
		return -1;
	}

	std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2];
	count = ut_kvp_getListCount(YAML::Node::p_instance,to_find_final_key.c_str());

	return count;

}
//  //
//  // // Function to get an integer value from a YAML node based on a path
int get_integer_value_from_profile( YAML::Node d_conf, const std::string& path){
	std::vector<std::string> segments = splitPath(path, '.');
	int count;
	if (segments.empty() || segments[0] != "components") {
		std::cerr << "Invalid path or incorrect format." << std::endl;
		return -1;		 
	}
	int total_componentes = ut_kvp_getListCount(YAML::Node::p_instance ,segments[0].c_str());
	if(total_componentes == 0){
		std::cerr << "components node not found or not a list." << std::endl;
		return -1;
	}
	bool found = false;
	int i = 0;
	char result_kvp[UT_KVP_MAX_ELEMENT_SIZE];
		//we have node lets find propername
	for(; i < total_componentes;i++){
	
	std::string to_find_key = segments[0] +"/"+ std::to_string(i) + "/" + "name";//segments[1];
		if(0 == ut_kvp_getStringField(YAML::Node::p_instance, to_find_key.c_str(), result_kvp, UT_KVP_MAX_ELEMENT_SIZE)){
		
			if(segments[1].compare(result_kvp) == 0){
				found = true;
				break;		//check the count no
			}
		}
	}
	if(!found){
		std::cerr << "Component with name " << segments[1] << " not found." << std::endl;
		return -1;
	}
	std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2] ;
	count = ut_kvp_getListCount(YAML::Node::p_instance,to_find_final_key.c_str());

	if(count > std::stoi(segments[3])){
		std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2] + "/" + segments[3] +"/"+segments[4];
		count = static_cast<int>(ut_kvp_getUInt32Field(YAML::Node::p_instance,to_find_final_key.c_str()));
		return count;
	}
	else{
		return -1;
	}

}
//   //
//   // // Function to extract a string value based on the path
std::string get_string_value_from_profile( YAML::Node d_conf, const std::string& path){

        std::vector<std::string> segments = splitPath(path, '.');
        int count;
        bool ret;
        if (segments.empty() || segments[0] != "components") {
                std::cerr << "Invalid path or incorrect format." << std::endl;
                return "";
        }
        int total_componentes = ut_kvp_getListCount(YAML::Node::p_instance ,segments[0].c_str());
        if(total_componentes == 0){
                std::cerr << "components node not found or not a list." << std::endl;
                return "";
        }
        bool found = false;
        int i = 0;
        char result_kvp[UT_KVP_MAX_ELEMENT_SIZE];
                //we have node lets find propername
        for(; i < total_componentes;i++){

        std::string to_find_key = segments[0] +"/"+ std::to_string(i) + "/" + "name";//segments[1];
                if(0 == ut_kvp_getStringField(YAML::Node::p_instance, to_find_key.c_str(), result_kvp, UT_KVP_MAX_ELEMENT_SIZE)){

                        if(segments[1].compare(result_kvp) == 0){
                                found = true;
                                break;          //check the count no
                        }
                }
        }
        if(!found){
                std::cerr << "Component with name " << segments[1] << " not found." << std::endl;
                return "";
        }
        std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2] ;
        count = ut_kvp_getListCount(YAML::Node::p_instance,to_find_final_key.c_str());
        //check is entry is vialbel in yaml then try to get value
        if(count > std::stoi(segments[3])){
                std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2] + "/" + segments[3] +"/"+segments[4];
		char result_kvp[UT_KVP_MAX_ELEMENT_SIZE];
                if(0 == ut_kvp_getStringField(YAML::Node::p_instance,to_find_final_key.c_str(),result_kvp, UT_KVP_MAX_ELEMENT_SIZE)){
			std::string str(result_kvp);
			return str;	
		}
		else{
                	return "";
		}
        }
        else{
		return "";
        }

}
//    //
//    // // Function to extract a boolean value based on the path
bool get_boolean_value_from_profile( YAML::Node d_conf, const std::string& path){
	std::vector<std::string> segments = splitPath(path, '.');
	int count;
	bool ret;	
	if (segments.empty() || segments[0] != "components") {
		std::cerr << "Invalid path or incorrect format." << std::endl;
		return false;
	}
	int total_componentes = ut_kvp_getListCount(YAML::Node::p_instance ,segments[0].c_str());
	if(total_componentes == 0){
		std::cerr << "components node not found or not a list." << std::endl;
		return false;
	}
	bool found = false;
	int i = 0;
	char result_kvp[UT_KVP_MAX_ELEMENT_SIZE];
		//we have node lets find propername
	for(; i < total_componentes;i++){
	
	std::string to_find_key = segments[0] +"/"+ std::to_string(i) + "/" + "name";
		if(0 == ut_kvp_getStringField(YAML::Node::p_instance, to_find_key.c_str(), result_kvp, UT_KVP_MAX_ELEMENT_SIZE)){
		
			if(segments[1].compare(result_kvp) == 0){
				found = true;
				break;		//check the count no
			}
		}
	}
	if(!found){
		std::cerr << "Component with name " << segments[1] << " not found." << std::endl;
		return false;
	}
	std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2] ;
	count = ut_kvp_getListCount(YAML::Node::p_instance,to_find_final_key.c_str());
	//check is entry is vialbel in yaml then try to get value
	if(count > std::stoi(segments[3])){
		std::string to_find_final_key = segments[0] +"/"+ std::to_string(i) + "/" + segments[2] + "/" + segments[3] +"/"+segments[4];
		ret = ut_kvp_getBoolField(YAML::Node::p_instance,to_find_final_key.c_str());
		return ret;
	}
	else{
		return false;
	}

	return false;
}

