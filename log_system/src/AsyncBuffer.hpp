#pragma once

#include <cassert>
#include <string>
#include <vector>
#include <cmath>

#include <Util.hpp>

namespace asynclog
{

class Buffer
{
protected:
    std::vector<char>buffer_;   //缓冲区
    size_t write_pos_;          //生产者所在的位置
    size_t read_pos_;           //消费者所在的位置
    Util::JsonUtil::JsonData config_data_;

    void ensureEnoughSpace(size_t len)
    {
        if(writeableBytes()<len)
        {
            makeSpace(len);
        }
    }

    void makeSpace(size_t len)
    {
        //如果可读区域左侧有空间，则移动可读空间
        if(read_pos_>0)
        {
            std::copy(peek(),peek()+readableBytes(),buffer_.begin());
            size_t readable_bytes=readableBytes();
            read_pos_=0;
            write_pos_=readable_bytes;
        }

        //如果移动空间之后还是小，则扩容
        if(writeableBytes()<len)
        {
            //如果整个缓冲区大小小于threshold_，选择成倍扩容，否则选择线性扩容，避免缓冲区过度膨胀
            if(buffer_.size()>=config_data_.threshold_)
            {
                int times=len/config_data_.linear_growth_+1;
                buffer_.resize(buffer_.size()+times*config_data_.linear_growth_);
            }
            else
            {
                size_t new_size=len + write_pos_;//目前已占用的空间加上len的大小
                size_t next_power_of_2=buffer_.size();
                while(next_power_of_2<new_size)
                {
                    next_power_of_2*=2;
                }
                buffer_.resize(next_power_of_2);
            }
        }

    }
    
public:
    Buffer(const Util::JsonUtil::JsonData& config_data)
        :config_data_(config_data)
        ,buffer_(config_data.buffer_size_,0)
        ,write_pos_(0)
        ,read_pos_(0)
    {}

    //返回可读的字节数
    inline size_t readableBytes()const { return write_pos_-read_pos_;}

    //返回可写的字节数
    inline size_t writeableBytes()const { return buffer_.size()-write_pos_;}

    //返回可读缓冲区的起始地址，内容只读
    inline const char* peek(){return &buffer_[read_pos_];}

    //确保可读缓冲区中有len字节的数据，返回一个可写的指针
    char* readBegin(size_t len)
    {
        if(len>readableBytes())
        {
            return nullptr;
        }
        return &buffer_[read_pos_];
    }

    inline bool isEmpty()const {return write_pos_==read_pos_;}

    void moveReadPos(size_t len)
    {
        if(len>readableBytes())
        {
            read_pos_=write_pos_;
        }
        else 
        {
            read_pos_+=len;
        }
    }

    void moveWritePos(size_t len)
    {
        if(len>writeableBytes())
        {
            write_pos_=buffer_.size();
        }
        else
        {
            write_pos_+=len;
        }
    }

    void push(const char* data,size_t len)
    {
        if(data==nullptr) return;
        ensureEnoughSpace(len);
        std::copy(data,data+len,buffer_.begin()+write_pos_);
        write_pos_+=len;
    }

    inline void reset()
    {
        read_pos_=0;
        write_pos_=0;
    }

    void swap(Buffer& buffer)
    {
        this->buffer_.swap(buffer.buffer_);
        std::swap(this->read_pos_,buffer.read_pos_);
        std::swap(this->write_pos_,buffer.write_pos_);
    }

    inline size_t size() const
    {
        return buffer_.size();
    }
};
    
} // namespace asynclog
