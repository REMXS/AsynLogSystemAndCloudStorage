#include <cstdio>
#include <string>
#include <ctime>

class ISystemOps {
public:
    virtual ~ISystemOps() = default;

    virtual FILE* fopen(const char* filename, const char* mode) = 0;
    virtual int fclose(FILE* stream) = 0;
    virtual size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) = 0;
    virtual int fflush(FILE* stream) = 0;
    virtual int fileno(FILE* stream) = 0;
    virtual int fsync(int fd) = 0;
    virtual int ferror(FILE* stream) = 0;
    
    virtual void perror(const char* s) = 0;

    virtual void createDirectory(const std::string& path) = 0;
    virtual time_t now() = 0; // 获取当前时间

};