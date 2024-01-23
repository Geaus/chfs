#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

#include "map_reduce/protocol.h"

namespace mapReduce {

    Worker::Worker(MR_CoordinatorConfig config) {
        mr_client = std::make_unique<chfs::RpcClient>(config.ip_address, config.port, true);
        outPutFile = config.resultFile;
        chfs_client = config.client;
        work_thread = std::make_unique<std::thread>(&Worker::doWork, this);
        // Lab4: Your code goes here (Optional).
    }

    void Worker::doMap(int index, const std::string &filename) {
        // Lab4: Your code goes here.

        auto lookup_res = chfs_client->lookup(1,filename);
        auto inode_id = lookup_res.unwrap();

        auto type_attri = chfs_client->get_type_attr(inode_id);
        auto length = type_attri.unwrap().second.size;

        auto read_res = chfs_client->read_file(inode_id,0,length);
        auto char_vec = read_res.unwrap();

        std::string content(char_vec.begin(), char_vec.end());
        std::vector <KeyVal> inter_kvs = Map(content);

        std::vector <std::string> inter_file_content(4);
        for (const auto &entry : inter_kvs) {

            int i = hash(entry.key);

            inter_file_content[i].append(entry.key + ' ' + entry.val + '\n');
        }

        for (int reducer_id = 0; reducer_id < this->reducer_num; reducer_id++) {
            auto inter_content = inter_file_content[reducer_id];

            if (!inter_content.empty()) {

                std::string inter_file_name = "mr-" + std::to_string(index) + "-" + std::to_string(reducer_id);

                auto mknode_res = chfs_client->mknode(chfs::ChfsClient::FileType::REGULAR, 1,
                                                  inter_file_name);
                auto new_inode_id = mknode_res.unwrap();

                std::vector<chfs::u8> output(inter_content.begin(), inter_content.end());
                chfs_client->write_file(new_inode_id,0,output);
            }
        }

    }

    void Worker::doReduce(int index, int nfiles) {
        // Lab4: Your code goes here.

        std::string inter_file_name;
        std::unordered_map<std::string, int> word_count;

        for (int mapper_id = 0; mapper_id < nfiles; mapper_id++) {

            inter_file_name = "mr-" + std::to_string(mapper_id) + '-' + std::to_string(index);

            auto lookup_res = chfs_client->lookup(1, inter_file_name);
            auto inode_id = lookup_res.unwrap();

            auto type_attri = chfs_client->get_type_attr(inode_id);
            auto length = type_attri.unwrap().second.size;

            auto read_res = chfs_client->read_file(inode_id,0,length);
            auto char_vec = read_res.unwrap();

            std::string tmp_content(char_vec.begin(), char_vec.end());
            std::stringstream ss(tmp_content);

            std::string key, value;
            while (ss >> key >> value) {
                word_count[key] += std::stoi(value);
            }
        }

        std::string output_content;
        for (const auto &entry : word_count){
            output_content.append(entry.first + ' ' + std::to_string(entry.second) + '\n') ;
        }

        auto lookup_res = chfs_client->lookup(1,outPutFile);
        auto inode_id = lookup_res.unwrap();

        auto type_attri = chfs_client->get_type_attr(inode_id);
        auto length = type_attri.unwrap().second.size;

        std::vector<chfs::u8> output(output_content.begin(), output_content.end());
        chfs_client->write_file(inode_id,length,output);

    }

    void Worker::doSubmit(mr_tasktype taskType, int index) {
        // Lab4: Your code goes here.
        mr_client->call(SUBMIT_TASK,(int)taskType,index);
    }

    void Worker::stop() {
        shouldStop = true;
        work_thread->join();
    }

    void Worker::doWork() {
        while (!shouldStop) {
            // Lab4: Your code goes here.

            auto call_ret = mr_client->call(ASK_TASK,1);
            auto reply = call_ret.unwrap()->as<AskTaskReply>();

            if(this->mapper_num == 0){
               mapper_num = reply.mapper_num;

            }
            if(this->reducer_num == 0){
               reducer_num = reply.reducer_num;
            }

            switch(reply.taskType){
                case MAP:
                    doMap(reply.index, reply.filename);
                    doSubmit(MAP, reply.index);
                    break;
                case REDUCE:
                    doReduce(reply.index, reply.mapper_num);
                    doSubmit(REDUCE, reply.index);
                    break;
                case NONE:
                    sleep(1);
                    break;
            }
        }
    }
}