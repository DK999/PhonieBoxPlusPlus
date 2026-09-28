#pragma once

#include <string>

class Irfid_reader {
  public:
    virtual bool init() = 0;
    virtual std::string getID() = 0;
    virtual void readFromDevice() = 0;
    virtual void stop() = 0;
    virtual ~Irfid_reader() = default;
};
