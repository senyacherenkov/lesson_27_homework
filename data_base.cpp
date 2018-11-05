#include "data_base.h"
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <iostream>

namespace  {
    constexpr const char*   POSITIVE_MARK     = "OK\n";
    constexpr const char*   ERROR_MARK        = "ERR";
    constexpr const char*   ERR_WRONG_FORMAT  = "ERR WRONG FORMAT\n";
    constexpr const char*   EMPTY_RESULT      = "";
    constexpr int           THREAD_NUMBER     = 5;
    constexpr int           MSG_MAX_LENGTH    = 4;

    bool isValidIndex(std::string &index)
    {
        bool result = false;
        for(const auto& part: index)
            result = std::isdigit(part);
        return result;
    }

    std::string errorMsgNoTable(std::string &table)
    {
        std::string errorMsg;
        errorMsg = ERROR_MARK;
        errorMsg += " table ";
        errorMsg += table;
        errorMsg += " doesn't exists";
        errorMsg += "\n";
        return errorMsg;
    }

    std::string errorUnknownCommand(std::string &command)
    {
        std::string errorMsg;
        errorMsg = ERROR_MARK;
        errorMsg += " unknown command ";
        errorMsg += command;
        errorMsg += "\n";
        return errorMsg;
    }

    std::string errorDuplicateData(std::string &index)
    {
        std::string errorMsg;
        errorMsg = ERROR_MARK;
        errorMsg += " duplicate ";
        errorMsg += index;
        errorMsg += "\n";
        return errorMsg;
    }
}


DataBase::DataBase()
{
    m_over.store(false, std::memory_order_relaxed);

    for(size_t i = 0; i < THREAD_NUMBER; i++)
        m_workers.emplace_back(
                    [this]
                        {
                            while (true) {
                                std::unique_lock<std::mutex> lck{m_taskMutex};

                                while (!m_over.load(std::memory_order_relaxed) && m_taskQueue.empty())
                                    m_condition.wait(lck);

                                if (m_over.load(std::memory_order_relaxed))
                                    break;

                                auto task = m_taskQueue.front();
                                m_taskQueue.pop();

                                lck.unlock();

                                task();
                            }

                            while(!m_taskQueue.empty()) {
                                std::unique_lock<std::mutex> lck{m_taskMutex};
                                auto task = m_taskQueue.front();
                                m_taskQueue.pop();

                                lck.unlock();

                                task();
                            }
        });
}

DataBase::~DataBase()
{
    m_over.store(true, std::memory_order_relaxed);
    m_condition.notify_all();
    for(std::thread& worker: m_workers){
        worker.join();
    }
}

std::vector<std::string> DataBase::prepareData(const char *data)
{
    std::istringstream input(data);
    std::string word;
    std::vector<std::string> preparedData;

    while (input >> word)
        preparedData.push_back(word);

    return preparedData;
}

size_t DataBase::newDataHandling(const char *data)
{
    size_t id = 0;
    {
        std::lock(m_taskMutex, m_dataMutex);
        std::lock_guard<std::mutex> lck1(m_taskMutex, std::adopt_lock);
        std::lock_guard<std::mutex> lck2(m_dataMutex, std::adopt_lock);
        std::lock_guard<std::mutex> lck3(m_waitMutex, std::adopt_lock);

        if(!m_freeContextIDs.empty()) {

            id = m_freeContextIDs.front();
            m_freeContextIDs.pop();

            *(std::next(m_contexts.begin(), static_cast<long>(id))) = EMPTY_RESULT;
        }
        else  {
            m_contexts.push_back(EMPTY_RESULT);
            id = m_contexts.size() - 1;
        }
    }
    auto dataVector = prepareData(data);

    auto task = [dataVector, id, this]()->bool {
                                            return parse(dataVector, id);
                                       };

    {
        std::lock(m_taskMutex, m_dataMutex);
        std::lock_guard<std::mutex> lck1(m_taskMutex, std::adopt_lock);
        std::lock_guard<std::mutex> lck2(m_dataMutex, std::adopt_lock);
        std::lock_guard<std::mutex> lck3(m_waitMutex, std::adopt_lock);

        m_taskQueue.push(task);

    }

    m_condition.notify_all();
    return id;
}

std::string DataBase::getResult(size_t id)
{
    std::unique_lock<std::mutex> lck(m_waitMutex);
    do{
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while(*(std::next(m_contexts.begin(), static_cast<long>(id))) == EMPTY_RESULT);

    lck.unlock();
    std::string temp = *(std::next(m_contexts.begin(), static_cast<long>(id)));
    lck.lock();

    *(std::next(m_contexts.begin(), static_cast<long>(id))) = EMPTY_RESULT;
    m_freeContextIDs.push(id);
    return temp;
}

bool DataBase::parse(std::vector<std::string> data, size_t id)
{
    {
        std::unique_lock<std::mutex> lck(m_dataMutex);
        *(std::next(m_contexts.begin(), static_cast<long>(id))) = POSITIVE_MARK;

        if(data.size() == 0) {
            return true;
        } else if (data.size() > MSG_MAX_LENGTH) {
            *(std::next(m_contexts.begin(), static_cast<long>(id))) = ERR_WRONG_FORMAT;
            return false;
        }
    }

    if(data.at(0) == "INSERT") {
        if((data.size() == 4) && isValidIndex(data.at(2))) {
            int index = std::stoi(data.at(2));
            if(data.at(1) == "A") {
                std::unique_lock<std::mutex> lck(m_dataMutex);
                auto it = m_tableA.emplace(index, data.at(3));
                if(!it.second) {
                    *(std::next(m_contexts.begin(), static_cast<long>(id))) = errorDuplicateData(data.at(2));
                    return false;
                }
            }
            else if(data.at(1) == "B") {
                std::unique_lock<std::mutex> lck(m_dataMutex);
                auto it = m_tableB.emplace(index, data.at(3));
                if(!it.second) {
                    *(std::next(m_contexts.begin(), static_cast<long>(id))) = errorDuplicateData(data.at(2));
                    return false;
                }
            }
            else {
                std::unique_lock<std::mutex> lck(m_dataMutex);
                *(std::next(m_contexts.begin(), static_cast<long>(id))) = errorMsgNoTable(data.at(1));
                return false;
            }
        } else {
            std::unique_lock<std::mutex> lck(m_dataMutex);
            *(std::next(m_contexts.begin(), static_cast<long>(id))) = ERR_WRONG_FORMAT;
            return false;
        }
    }
    else if (data.at(0) == "TRUNCATE") {
        if((data.size() == 2)) {
            if(data.at(1) == "A") {
                std::unique_lock<std::mutex> lck(m_dataMutex);
                m_tableA.clear();
            }
            else if(data.at(1) == "B") {
                std::unique_lock<std::mutex> lck(m_dataMutex);
                m_tableB.clear();
            }
            else {
                std::unique_lock<std::mutex> lck(m_dataMutex);
                *(std::next(m_contexts.begin(), static_cast<long>(id))) = errorMsgNoTable(data.at(1));
                return false;
            }
        } else {
            std::unique_lock<std::mutex> lck(m_dataMutex);
            *(std::next(m_contexts.begin(), static_cast<long>(id))) = ERR_WRONG_FORMAT;
            return false;
        }
    }
    else if(data.at(0) == "INTERSECTION")
        createIntersection(id);
    else if(data.at(0) == "SYMMETRIC_DIFFERENCE")
        createSymmetricDifference(id);
    else {
        std::unique_lock<std::mutex> lck(m_dataMutex);
        *(std::next(m_contexts.begin(), static_cast<long>(id))) = errorUnknownCommand(data.at(0));
        return false;
    }

    return true;
}

void DataBase::createIntersection(size_t id)
{
    std::set<int> indexesA;
    std::set<int> indexesB;

    {
        std::unique_lock<std::mutex> lck(m_dataMutex);
        if(m_tableA.empty() || m_tableB.empty())
        {
            *(std::next(m_contexts.begin(), static_cast<long>(id))) = POSITIVE_MARK;
            return;
        }
        for(const auto& element: m_tableA)
            indexesA.insert(element.first);

        for(const auto& element: m_tableB)
            indexesB.insert(element.first);
    }

    std::vector<int> resultIntersection;
    std::string serializedResult("");

    std::set_intersection(indexesA.begin(), indexesA.end(), indexesB.begin(), indexesB.end(),
                          std::inserter(resultIntersection, resultIntersection.begin()));

    for(const auto& index: resultIntersection)
    {
        serializedResult += std::to_string(index);
        serializedResult += ",";
        serializedResult += m_tableA[index];
        serializedResult += ",";
        serializedResult += m_tableB[index];
        serializedResult += "\n";
    }
    serializedResult += POSITIVE_MARK;

    std::unique_lock<std::mutex> lck(m_dataMutex);
    *(std::next(m_contexts.begin(), static_cast<long>(id))) = serializedResult;
}

void DataBase::createSymmetricDifference(size_t id)
{
    std::set<int> indexesA;
    std::set<int> indexesB;

    {
        std::unique_lock<std::mutex> lck(m_dataMutex);
        if(m_tableA.empty() || m_tableB.empty())
        {
            *(std::next(m_contexts.begin(), static_cast<long>(id))) = POSITIVE_MARK;
            return;
        }
        for(const auto& element: m_tableA)
            indexesA.insert(element.first);

        for(const auto& element: m_tableB)
            indexesB.insert(element.first);
    }

    std::vector<int> resultDiff;
    std::string serializedResult("");

    std::set_symmetric_difference(indexesA.begin(), indexesA.end(), indexesB.begin(), indexesB.end(),
                        std::inserter(resultDiff, resultDiff.begin()));

    {        
        for(const auto& index: resultDiff)
        {
            serializedResult += std::to_string(index);
            serializedResult += ",";
            if(m_tableA.count(index))
                serializedResult += m_tableA[index];
            serializedResult += ",";
            if(m_tableB.count(index))
                serializedResult += m_tableB[index];
            serializedResult += "\n";
        }
        serializedResult += POSITIVE_MARK;
    }

    std::unique_lock<std::mutex> lck(m_dataMutex);
    *(std::next(m_contexts.begin(), static_cast<long>(id))) = serializedResult;
}


