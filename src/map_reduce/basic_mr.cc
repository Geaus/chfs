#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

#include "map_reduce/protocol.h"

namespace mapReduce{
//
// The map function is called once for each file of input. The first
// argument is the name of the input file, and the second is the
// file's complete contents. You should ignore the input file name,
// and look only at the contents argument. The return value is a slice
// of key/value pairs.
//
    std::vector<KeyVal> Map(const std::string &content) {
        // Your code goes here
        // Hints: split contents into an array of words.
        std::vector <KeyVal> res;
        std::string word;
        std::unordered_map<std::string, int> word_count;

        for (const auto &c : content) {
            if (!isCharacter(c) && !word.empty()){
                word_count[word] += 1;
                word.clear();
            }
            else if (isCharacter(c)) {
                word += c;
            }
        }
        for (const auto &pair : word_count) {
            res.emplace_back(pair.first, std::to_string(pair.second));
        }
        return res;
    }

//
// The reduce function is called once for each key generated by the
// map tasks, with a list of all the values created for that key by
// any map task.
//
    std::string Reduce(const std::string &key, const std::vector<std::string> &values) {
        // Your code goes here
        // Hints: return the number of occurrences of the word.
        int sum = 0;
        std::string res;

        for (const auto &value : values) {
            sum += std::stoi(value);
        }
        res = std::to_string(sum);
        return res;
    }
}