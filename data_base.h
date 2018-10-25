#pragma once
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <list>
#include <atomic>

class DataBase {
    using TTaskCallback = std::function<size_t()>;
public:
    DataBase();
    ~DataBase();

    size_t newDataHandling(const char* data);
    std::string getResult(size_t id);

    bool parse(std::vector<std::string> data, size_t id);
    std::vector<std::string> prepareData(const char *data);

private:
    void createIntersection(size_t id);
    void createSymmetricDifference(size_t id);
private:
    std::map<int, std::string>  m_tableA;
    std::map<int, std::string>  m_tableB;

    std::list<std::string>      m_contexts;
    std::queue<size_t>          m_freeContextIDs;

    std::vector<std::thread>    m_workers;
    std::mutex                  m_taskMutex;
    std::mutex                  m_dataMutex;
    std::mutex                  m_waitMutex;
    std::condition_variable     m_condition;
    std::atomic_bool            m_over;

    std::queue<TTaskCallback>   m_taskQueue;
};

