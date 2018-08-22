/*
    author: linukey
    time: 2017.11.12
*/

#include "../../include/utils/string_utils.h"

using namespace std;

namespace linukey {
namespace webserver {
namespace utils {

//根据 key 作为分隔符，key 个数不限,只要是连在一块的，都算作分隔符
void split_by_key(const string& str, const char& key, vector<string>& result){
    result.clear();
    size_t pos = 0, pre_pos = 0;
    while((pos = str.find(key, pre_pos)) != string::npos){
        if (pre_pos == pos){
            ++pre_pos;
            continue;
        }
        result.push_back(str.substr(pre_pos, pos-pre_pos));
        pre_pos = pos;
        do{
            pre_pos = pre_pos+1;
            if (pre_pos >= str.length()) break;
        } while (str[pre_pos] == key);
    }
    if (pre_pos < str.length())
        result.push_back(str.substr(pre_pos));
}

string to_lower(const string& str){
    string result;
    transform(str.begin(), str.end(), back_inserter(result), ::tolower); 
    return result;
}

}
}
}
