/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Created by edgar costa molero on 01.05.18.
//

#include "nodes-usage.h"
#include "utils.h"

/*
 * Nodes Usage Object
 */


NS_LOG_COMPONENT_DEFINE ("nodes-usage");


namespace ns3 {

    void NodesUsage::UpdateRxReceivers(Ptr<Node> node, uint64_t flowSize) {

        std::string node_name = GetNodeName(node);

        if (this->receivers.count(node_name) > 0) {
            this->receivers[node_name].rxBytes += flowSize;
            this->receivers[node_name].rxFlows += 1;
        } else {
            this->receivers[node_name].rxBytes = flowSize;
            this->receivers[node_name].rxFlows = 1;
            this->receivers[node_name].ipAddr = Ipv4AddressToString(GetNodeIp(node));
            this->receivers[node_name].name = node_name;
        }
    }

    void NodesUsage::UpdateTxSenders(Ptr<Node> node, uint64_t flowSize) {

        std::string node_name = GetNodeName(node);

        if (this->senders.count(node_name) > 0) {
            this->senders[node_name].txBytes += flowSize;
            this->senders[node_name].txFlows += 1;
        } else {
            this->senders[node_name].txBytes = flowSize;
            this->senders[node_name].txFlows = 1;
            this->senders[node_name].ipAddr = Ipv4AddressToString(GetNodeIp(node));
            this->senders[node_name].name = node_name;
        }
    }

    std::vector<UsageStruct> NodesUsage::GetReceiversVector(void) {
        return receiversVector;
    }


    void NodesUsage::MapToVector(std::unordered_map<std::string, UsageStruct> &map, std::vector<UsageStruct> &vector) {

        std::unordered_map<std::string, UsageStruct>::iterator it = map.begin();
        while (it != map.end()) {
            vector.push_back(it->second);
            it++;
        }
    }

    void NodesUsage::SortVector(std::vector<UsageStruct> &vector, std::string sort_by) {

        if (sort_by == "txFlows") {
            std::sort(vector.begin(), vector.end(), LessTxFlows());
        } else if (sort_by == "rxFlows") {
            std::sort(vector.begin(), vector.end(), LessRxFlows());
        } else if (sort_by == "txBytes") {
            std::sort(vector.begin(), vector.end(), LessTxBytes());
        } else if (sort_by == "rxBytes") {
            std::sort(vector.begin(), vector.end(), LessRxBytes());
        }
    }


    NodesUsage::NodesUsage(void) {
        /*nothing*/
    }

    void NodesUsage::Update(Ptr<Node> src, Ptr<Node> dst, uint64_t flowSize) {
        UpdateTxSenders(src, flowSize);
        UpdateRxReceivers(dst, flowSize);
    }


    void NodesUsage::SaveVectorInFile(std::string output_file, std::vector<UsageStruct> vector) {

        std::ofstream out_file(output_file);
        for (const auto &e : vector) {
            out_file << e.name << " ";
            out_file << e.ipAddr << " ";
            out_file << e.txFlows << " ";
            out_file << e.rxFlows << " ";
            out_file << e.txBytes << " ";
            out_file << e.rxBytes << "\n";
        }
    }

    void NodesUsage::Print(std::vector<UsageStruct> vector) {

        for (const auto &e : vector) {
            std::cout << e.name << " ";
            std::cout << e.ipAddr << " ";
            std::cout << e.txFlows << " ";
            std::cout << e.rxFlows << " ";
            std::cout << e.txBytes << " ";
            std::cout << e.rxBytes << "\n";
        }
        std::cout << "\n";
    }

    uint64_t NodesUsage::GetTotalRx(void) {
        int64_t sum = 0;
        for (const auto &e: receiversVector) {
            sum += e.rxBytes;
        }
        return sum;
    }

    void NodesUsage::Save(std::string output_file, bool sorted_by_flows, bool sorted_by_bytes) {

        MapToVector(receivers, receiversVector);
        MapToVector(senders, sendersVector);

        if (sorted_by_bytes | sorted_by_flows) {
            if (sorted_by_bytes) {
                SortVector(receiversVector, "rxBytes");
                SortVector(sendersVector, "txBytes");
            } else if (sorted_by_flows) {
                SortVector(receiversVector, "rxFlows");
                SortVector(sendersVector, "txFlows");
            }
        }

        SaveVectorInFile(output_file + "-tx.txt", sendersVector);
        SaveVectorInFile(output_file + "-rx.txt", receiversVector);
    }
}
/*
 *
 */