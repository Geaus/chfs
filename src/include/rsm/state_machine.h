#pragma once

#include <vector>
#include "common/config.h"

namespace chfs {

class ChfsCommand {
public:
    virtual ~ChfsCommand() {}

    virtual size_t size() const = 0;
    virtual std::vector<u8> serialize(int size) const = 0;
    virtual void deserialize(std::vector<u8> data, int size) = 0;
};

class ChfsStateMachine {
public:
    virtual ~ChfsStateMachine() {}

    /* Apply a log to the state machine. */
    virtual void apply_log(ChfsCommand &) = 0;

    /* Generate a snapshot of the current state. */
    virtual std::vector<u8> snapshot() = 0;
    
    /* Apply the snapshot to the state mahine. */
    virtual void apply_snapshot(const std::vector<u8> &) = 0;
};

} // namespace chfs