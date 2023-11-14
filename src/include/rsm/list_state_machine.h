#include "rsm/state_machine.h"
#include <mutex>
#include <sstream>

namespace chfs {

class ListCommand: public ChfsCommand {
public:
    ListCommand() {}
    ListCommand(int v): value(v) {}
    virtual ~ListCommand() {}

    virtual size_t size() const override { return 4; }

    virtual std::vector<u8> serialize(int sz) const override
    {
        std::vector<u8> buf;

        if (sz != size())
            return buf;
        
        buf.push_back((value >> 24) & 0xff);
        buf.push_back((value >> 16) & 0xff);
        buf.push_back((value >> 8) & 0xff);
        buf.push_back(value & 0xff);

        return buf;
    }

    virtual void deserialize(std::vector<u8> data, int sz) override
    {
        if (sz != size())
            return;
        value = (data[0] & 0xff) << 24;
        value |= (data[1] & 0xff) << 16;
        value |= (data[2] & 0xff) << 8;
        value |= data[3] & 0xff;
    }

    int value;
};

class ListStateMachine: public ChfsStateMachine {
public:
    ListStateMachine()
    {
        store.push_back(0);
        num_append_logs = 0;
    }

    virtual ~ListStateMachine() {}

    virtual std::vector<u8> snapshot() override
    {
        std::unique_lock<std::mutex> lock(mtx);
        std::vector<u8> data;
        std::stringstream ss;
        ss << (int)store.size();
        for (auto value : store)
            ss << ' ' << value;
        std::string str = ss.str();
        data.assign(str.begin(), str.end());
        return data;
    }

    virtual void apply_log(ChfsCommand &cmd) override
    {
        std::unique_lock<std::mutex> lock(mtx);
        const ListCommand &list_cmd = dynamic_cast<const ListCommand &>(cmd);
        store.push_back(list_cmd.value);
        num_append_logs++;
    }

    virtual void apply_snapshot(const std::vector<u8> &snapshot) override
    {
        std::unique_lock<std::mutex> lock(mtx);
        std::string str;
        str.assign(snapshot.begin(), snapshot.end());
        std::stringstream ss(str);
        store = std::vector<int>();
        int size;
        ss >> size;
        for (int i = 0; i < size; i++)
        {
            int temp;
            ss >> temp;
            store.push_back(temp);
        }
    }

    std::mutex mtx;
    std::vector<int> store;
    int num_append_logs;
};

}   /* namespace chfs */