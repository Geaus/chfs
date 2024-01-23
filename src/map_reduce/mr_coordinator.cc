#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mutex>

#include "map_reduce/protocol.h"

namespace mapReduce {
    auto Coordinator::askTask(int)->AskTaskReply {
        // Lab4 : Your code goes here.
        // Free to change the type of return value.
        AskTaskReply reply;
        mr_task_status new_task = assignTask();

        if (new_task.index == -1) {

            reply.taskType = NONE;
            reply.index = -1;
            reply.filename = "";
            std::cout << "coordinator: no task" << std::endl;

        }
        else {
            std::string filename = get_file_name(new_task.index);

            reply.taskType = new_task.taskType;
            reply.index = new_task.index;
            reply.filename = filename;

            reply.mapper_num = this->mapper_num;
            reply.reducer_num = this->reducer_num;
            std::cout << "coordinator: assign " << reply.filename << std::endl;
        }

        return reply;
    }

    mr_task_status Coordinator::assignTask() {
        std::unique_lock<std::mutex> uniqueLock(this->mtx);

        mr_task_status res_task;
        res_task.index = -1;

        if(this->completed_map < this->mapper_num){
            for(int index = 0; index < this->mapper_num; index++){
                auto new_task = this->map_task[index];
                if(!new_task.is_assigned){
                    this->map_task[index].is_assigned = true;

                    res_task.taskType = new_task.taskType;
                    res_task.index = new_task.index;
                    res_task.is_assigned = new_task.is_assigned;
                    res_task.is_completed = new_task.is_completed;

                    return res_task;
                }
            }
        }
        else{

            for(int index = 0; index < this->reducer_num; index++){
                auto new_task = this->reduce_task[index];
                if(!new_task.is_assigned){
                    this->reduce_task[index].is_assigned = true;

                    res_task.taskType = new_task.taskType;
                    res_task.index = new_task.index;
                    res_task.is_assigned = new_task.is_assigned;
                    res_task.is_completed = new_task.is_completed;

                    return res_task;
                }
            }
        }

        return res_task;
    }

    std::string Coordinator::get_file_name(int index) {
        std::unique_lock<std::mutex> uniqueLock(this->mtx);

        std::string file_name = this->files[index];
        return file_name;
    }

    int Coordinator::submitTask(int taskType, int index) {
        // Lab4 : Your code goes here.
        std::unique_lock<std::mutex> uniqueLock(this->mtx);

        switch (taskType) {
            case MAP:

                map_task[index].is_completed = true;
                this->completed_map++;
                std::cout << "coordinator: submit a map task " << index << std::endl;
                break;

            case REDUCE:

                reduce_task[index].is_completed = true;
                this->completed_reduce++;
                std::cout << "coordinator: submit a reduce task " << index << std::endl;
                break;

            default:
                break;
        }
        checkFinish();
        return 1;
        return 0;
    }

    void Coordinator::checkFinish(){

        if (this->completed_map >= this->mapper_num &&
        this->completed_reduce >= this->reducer_num) {
            this->isFinished = true;
        }

    }

    // mr_coordinator calls Done() periodically to find out
    // if the entire job has finished.
    bool Coordinator::Done() {
        std::unique_lock<std::mutex> uniqueLock(this->mtx);
        return this->isFinished;
    }

    // create a Coordinator.
    // nReduce is the number of reduce tasks to use.
    Coordinator::Coordinator(MR_CoordinatorConfig config, const std::vector<std::string> &files, int nReduce) {
        this->files = files;
        this->isFinished = false;
        // Lab4: Your code goes here (Optional).
        int file_num = (int )files.size();
        for (int i = 0; i < file_num; i++) {
            mr_task_status status(mr_tasktype::MAP,i, false, false);
            this->map_task.emplace_back(status);
        }
        for (int i = 0; i < nReduce; i++) {
            mr_task_status status(mr_tasktype::REDUCE,i, false, false);
            this->reduce_task.emplace_back(status);
        }

        this->mapper_num = file_num;
        this->reducer_num = nReduce;
        this->completed_map = 0;
        this->completed_reduce = 0;
    
        rpc_server = std::make_unique<chfs::RpcServer>(config.ip_address, config.port);
        rpc_server->bind(ASK_TASK, [this](int i) { return this->askTask(i); });
        rpc_server->bind(SUBMIT_TASK, [this](int taskType, int index) { return this->submitTask(taskType, index); });
        rpc_server->run(true, 1);
    }
}