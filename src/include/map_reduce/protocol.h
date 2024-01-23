#include <string>
#include <utility>
#include <vector>
#include <mutex>
#include "librpc/client.h"
#include "librpc/server.h"
#include "distributed/client.h"

//Lab4: Free to modify this file

namespace mapReduce {
    struct KeyVal {
        KeyVal(const std::string &key, const std::string &val) : key(key), val(val) {}
        KeyVal(){}
        std::string key;
        std::string val;
    };

    enum mr_tasktype {
        NONE = 0,
        MAP,
        REDUCE
    };
    struct mr_task_status {
        int taskType;
        int index;
        bool is_assigned;
        bool is_completed;

        mr_task_status(){}
        mr_task_status(int taskType,int index,bool is_assigned,bool is_completed):
        taskType(taskType),index(index),is_assigned(is_assigned),is_completed(is_completed){}
    };

    struct AskTaskReply {

        int taskType;
        int index;
        std::string filename;
        int mapper_num;
        int reducer_num;

        MSGPACK_DEFINE(
            taskType,
            index,
            filename,
            mapper_num,
            reducer_num
        )
        AskTaskReply(){}
        AskTaskReply(int taskType, int index, std::string filename, int mapper_num, int reducer_num):
                taskType(taskType), index(index), filename(std::move(filename)), mapper_num(mapper_num), reducer_num(reducer_num){}
    };

    inline bool isCharacter(char ch) {
        return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
    }

    inline int hash(const std::string &str) {
        unsigned int tmp = 0;
        for (const auto &c : str) {
            tmp = tmp * 127 + (int) c;
        }
        return (int)(tmp % 4);
    }

    std::vector<KeyVal> Map(const std::string &content);

    std::string Reduce(const std::string &key, const std::vector<std::string> &values);

    const std::string ASK_TASK = "ask_task";
    const std::string SUBMIT_TASK = "submit_task";

    struct MR_CoordinatorConfig {
        uint16_t port;
        std::string ip_address;
        std::string resultFile;
        std::shared_ptr<chfs::ChfsClient> client;

        MR_CoordinatorConfig(std::string ip_address, uint16_t port, std::shared_ptr<chfs::ChfsClient> client,
                             std::string resultFile) : port(port), ip_address(std::move(ip_address)),
                                                       resultFile(resultFile), client(std::move(client)) {}
    };

    class SequentialMapReduce {
    public:
        SequentialMapReduce(std::shared_ptr<chfs::ChfsClient> client, const std::vector<std::string> &files, std::string resultFile);
        void doWork();

    private:
        std::shared_ptr<chfs::ChfsClient> chfs_client;
        std::vector<std::string> files;
        std::string outPutFile;
    };

    class Coordinator {
    public:
        Coordinator(MR_CoordinatorConfig config, const std::vector<std::string> &files, int nReduce);
        auto askTask(int)->AskTaskReply;
        int submitTask(int taskType, int index);
        bool Done();
        mr_task_status assignTask();
        std::string get_file_name(int index);
        void checkFinish();

    private:
        std::vector<std::string> files;
        std::mutex mtx;
        bool isFinished;
        std::unique_ptr<chfs::RpcServer> rpc_server;

        int mapper_num;
        int reducer_num;
        std::vector <mr_task_status> map_task;
        std::vector <mr_task_status> reduce_task;
        long completed_map;
        long completed_reduce;
    };

    class Worker {
    public:
        explicit Worker(MR_CoordinatorConfig config);
        void doWork();
        void stop();

    private:
        void doMap(int index, const std::string &filename);
        void doReduce(int index, int nfiles);
        void doSubmit(mr_tasktype taskType, int index);

        std::string outPutFile;
        std::unique_ptr<chfs::RpcClient> mr_client;
        std::shared_ptr<chfs::ChfsClient> chfs_client;
        std::unique_ptr<std::thread> work_thread;
        bool shouldStop = false;

        int mapper_num = 0;
        int reducer_num = 0;
    };
}