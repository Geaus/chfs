#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "map_reduce/protocol.h"

namespace mapReduce {
    SequentialMapReduce::SequentialMapReduce(std::shared_ptr<chfs::ChfsClient> client,
                                             const std::vector<std::string> &files_, std::string resultFile) {
        chfs_client = std::move(client);
        files = files_;
        outPutFile = resultFile;
        // Your code goes here (optional)
    }

    void SequentialMapReduce::doWork() {
        // Your code goes here
        std::vector <KeyVal> inter_kvs;
        std::string outputs;

        std::cerr<<"1"<<std::endl;
        for(const auto &file : files){
            auto lookup_res = chfs_client->lookup(1,file);
            auto inode_id = lookup_res.unwrap();

            auto type_attri = chfs_client->get_type_attr(inode_id);
            auto length = type_attri.unwrap().second.size;

            auto read_res = chfs_client->read_file(inode_id,0,length);
            auto char_vec = read_res.unwrap();

            std::string content(char_vec.begin(), char_vec.end());
            std::vector <KeyVal> kvs = Map(content);
            inter_kvs.insert(inter_kvs.end(), kvs.begin(), kvs.end());
        }

        std::cerr<<"2"<<std::endl;
        sort(inter_kvs.begin(), inter_kvs.end(),[](KeyVal const & a, KeyVal const & b) {
                 return a.key < b.key ? true : false;});

        int i = 0;
        int j = 0;
        int size = (int )inter_kvs.size();

        while(i < size){
            j = i + 1;
            while( j < size && inter_kvs[i].key == inter_kvs[j].key ){
                j++;
            }

            std::vector <std::string> tmp_values;
            for(int k = i; k < j; k++){
                tmp_values.push_back(inter_kvs[k].val);
            }

            std::string output = Reduce(inter_kvs[i].key, tmp_values);
            outputs.append(inter_kvs[i].key + " " + output + "\n");

            i = j;
        }
        std::cerr<<"3"<<std::endl;

        auto out_inode = chfs_client->lookup(1,outPutFile);
        auto out_inode_id = out_inode.unwrap();

        std::vector<chfs::u8> content(outputs.begin(),outputs.end());
        chfs_client->write_file(out_inode_id, 0,content);
    }
}