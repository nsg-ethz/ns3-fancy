//
// Created by edgar costa molero on 04.05.18.
//

#ifndef FLOW_ERROR_MODEL_H
#define FLOW_ERROR_MODEL_H

#include <set>
#include "ns3/error-model.h"

namespace ns3 {

class Packet;


//Custom. TODO: Change descriptions

/**
* \brief Determine which packets are errored corresponding to an underlying
* distribution, rate, and unit.
*
* This object is used to flag packets as being lost/errored or not.
* The two parameters that govern the behavior are the rate (or
* equivalently, the mean duration/spacing between errors), and the
* unit (which may be per-bit, per-byte, and per-packet).
* Users can optionally provide a RandomVariableStream object; the default
* is to use a Uniform(0,1) distribution.

* Reset() on this model will do nothing
*
* IsCorrupt() will not modify the packet data buffer
*/
class FlowErrorModel : public ErrorModel
{
public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId (void);

    FlowErrorModel ();
    virtual ~FlowErrorModel ();

    /**
     * The error model can use l3 or l4 protocols to identify a flow
     */
    enum FlowLayer
    {
        L3_LAYER,
        L4_LAYER
    };

    /**
     * \returns the ErrorUnit being used by the underlying model
     */
    FlowErrorModel::FlowLayer GetLayer (void) const;
    /**
     * \param error_unit the ErrorUnit to be used by the underlying model
     */
    void SetLayer (enum FlowLayer flow_layer);

    /**
     * \returns the error rate being applied by the model
     */
    double GetRate (void) const;
    /**
     * \param rate the error rate to be used by the model
     */
    void SetRate (double rate);

    /**
     * \param ranvar A random variable distribution to generate random variates
     */
    void SetRandomVariable (Ptr<RandomVariableStream>);

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams (possibly zero) that
     * have been assigned.
     *
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
     */

    void SetNormalErrorModel(Ptr<ErrorModel> error_model);
    void SetNormalErrorModelAttribute (std::string name, const AttributeValue &value);
    int64_t AssignStreams (int64_t stream);

private:
    virtual bool DoCorrupt (Ptr<Packet> p);
    /**
     * Corrupt a packet (packet unit).
     * \param p the packet to corrupt
     * \returns true if the packet is corrupted
     */
    virtual void DoReset (void);

    bool DoCorruptFlow (uint64_t flow_id);
    uint64_t GetHeaderHash(Ptr<Packet> p);
    bool IsRed(uint64_t hash);
    bool IsGreen(uint64_t hash);

    enum FlowLayer m_layer; //!< Error rate unit
    double m_flowErrorRate; //!< Error rate
    Ptr<RandomVariableStream> m_ranvar; //!< rng stream
    std::set<uint64_t> m_redFlows; //! Set of flows that have to be failed
    std::set<uint64_t> m_greenFlows; //! Set of flows that will not be dropped
    Ptr<ErrorModel> m_normalErrorModel; //! Default Error model applyed to green packets
};

} // namespace ns3

#endif //FLOW_ERROR_MODEL_H
