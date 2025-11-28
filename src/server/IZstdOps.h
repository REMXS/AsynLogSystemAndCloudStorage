#pragma once
#include <string>
class IZipdOps
{
public:
    virtual bool compress(const std::string& input,std::string&output, int level=3)=0;
    virtual bool decompress(const std::string& input ,std::string&output)=0;

    virtual ~IZipdOps()=default;
};

