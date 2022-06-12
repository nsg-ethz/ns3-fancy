/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Created by edgar costa molero on 01.05.18.
//

#ifndef NODES_USAGE_H
#define NODES_USAGE_H

#include <unordered_map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"

namespace ns3 {


    struct UsageStruct {
        std::string name;
        std::string ipAddr;
        uint64_t rxFlows;
        uint64_t txFlows;
        uint64_t txBytes;
        uint64_t rxBytes;
    };

    struct LessRxFlows {
        bool operator()(const UsageStruct &x, const UsageStruct &y) const {
            return x.rxFlows < y.rxFlows;
        }
    };

    struct LessTxFlows {
        bool operator()(const UsageStruct &x, const UsageStruct &y) const {
            return x.txFlows < y.txFlows;
        }
    };

    struct LessRxBytes {
        bool operator()(const UsageStruct &x, const UsageStruct &y) const {
            return x.rxBytes < y.rxBytes;
        }
    };

    struct LessTxBytes {
        bool operator()(const UsageStruct &x, const UsageStruct &y) const {
            return x.txBytes < y.txBytes;
        }
    };

    class NodesUsage {

    private:

        std::unordered_map<std::string, UsageStruct> senders;
        std::unordered_map<std::string, UsageStruct> receivers;

        /*Have to flatten maps to sort them */
        std::vector<UsageStruct> sendersVector;
        std::vector<UsageStruct> receiversVector;

        void UpdateTxSenders(Ptr<Node> node, uint64_t flowSize);

        void UpdateRxReceivers(Ptr<Node> node, uint64_t flowSize);

        void SortVector(std::vector<UsageStruct> &vector, std::string sortby);

        void SaveVectorInFile(std::string outputFile, std::vector<UsageStruct> vector);

    public:

        NodesUsage(void);

        uint64_t GetTotalRx(void);

        std::vector<UsageStruct> GetReceiversVector(void);

        void Update(Ptr<Node> src, Ptr<Node> dst, uint64_t flow_size);

        void MapToVector(std::unordered_map<std::string, UsageStruct> &map, std::vector<UsageStruct> &vector);

        void Print(std::vector<UsageStruct> vector);

        /*Saves senders and receivers in two different files. Sorts using: num flows as default */
        void Save(std::string outputFile, bool sortedByFlows = true, bool sortedByBytes = false);

    };

}

#endif //NODES_USAGE_H
