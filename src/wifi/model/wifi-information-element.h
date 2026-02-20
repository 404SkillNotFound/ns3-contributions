/*
 * Copyright (c) 2010 Dean Armstrong
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Dean Armstrong <deanarm@gmail.com>
 */

#ifndef WIFI_INFORMATION_ELEMENT_H
#define WIFI_INFORMATION_ELEMENT_H

#include "ns3/header.h"

#include <optional>

namespace ns3
{

/// Size in bytes of the Element ID Extension field (IEEE 802.11-2020 9.4.2.1 General)
constexpr uint8_t WIFI_IE_ELEMENT_ID_EXT_SIZE = 1;

/**
 * This type is used to represent an Information Element ID. An
 * enumeration would be tidier, but doesn't provide for the
 * inheritance that is currently preferable to cleanly support
 * pre-standard modules such as mesh. Maybe there is a nice way of
 * doing this with a class.
 *
 * Until such time as a better way of implementing this is dreamt up
 * and applied, developers will need to be careful to avoid
 * duplication of IE IDs in the defines below (and in files which
 * declare "subclasses" of WifiInformationElement). Sorry.
 */
typedef uint8_t WifiInformationElementId;

/**
 * Here we have definition of all Information Element IDs in IEEE
 * 802.11-2007. See the comments for WifiInformationElementId - this could
 * probably be done in a considerably tidier manner.
 */
#define IE_SSID (static_cast<WifiInformationElementId>(0))
#define IE_SUPPORTED_RATES (static_cast<WifiInformationElementId>(1))
#define IE_FH_PARAMETER_SET (static_cast<WifiInformationElementId>(2))
#define IE_DSSS_PARAMETER_SET (static_cast<WifiInformationElementId>(3))
#define IE_CF_PARAMETER_SET (static_cast<WifiInformationElementId>(4))
#define IE_TIM (static_cast<WifiInformationElementId>(5))
#define IE_IBSS_PARAMETER_SET (static_cast<WifiInformationElementId>(6))
#define IE_COUNTRY (static_cast<WifiInformationElementId>(7))
#define IE_HOPPING_PATTERN_PARAMETERS (static_cast<WifiInformationElementId>(8))
#define IE_HOPPING_PATTERN_TABLE (static_cast<WifiInformationElementId>(9))
#define IE_REQUEST (static_cast<WifiInformationElementId>(10))
#define IE_BSS_LOAD (static_cast<WifiInformationElementId>(11))
#define IE_EDCA_PARAMETER_SET (static_cast<WifiInformationElementId>(12))
#define IE_TSPEC (static_cast<WifiInformationElementId>(13))
#define IE_TCLAS (static_cast<WifiInformationElementId>(14))
#define IE_SCHEDULE (static_cast<WifiInformationElementId>(15))
#define IE_CHALLENGE_TEXT (static_cast<WifiInformationElementId>(16))
// 17 to 31 are reserved
#define IE_POWER_CONSTRAINT (static_cast<WifiInformationElementId>(32))
#define IE_POWER_CAPABILITY (static_cast<WifiInformationElementId>(33))
#define IE_TPC_REQUEST (static_cast<WifiInformationElementId>(34))
#define IE_TPC_REPORT (static_cast<WifiInformationElementId>(35))
#define IE_SUPPORTED_CHANNELS (static_cast<WifiInformationElementId>(36))
#define IE_CHANNEL_SWITCH_ANNOUNCEMENT (static_cast<WifiInformationElementId>(37))
#define IE_MEASUREMENT_REQUEST (static_cast<WifiInformationElementId>(38))
#define IE_MEASUREMENT_REPORT (static_cast<WifiInformationElementId>(39))
#define IE_QUIET (static_cast<WifiInformationElementId>(40))
#define IE_IBSS_DFS (static_cast<WifiInformationElementId>(41))
#define IE_ERP_INFORMATION (static_cast<WifiInformationElementId>(42))
#define IE_TS_DELAY (static_cast<WifiInformationElementId>(43))
#define IE_TCLAS_PROCESSING (static_cast<WifiInformationElementId>(44))
#define IE_HT_CAPABILITIES (static_cast<WifiInformationElementId>(45))
#define IE_QOS_CAPABILITY (static_cast<WifiInformationElementId>(46))
// 47 is reserved
#define IE_RSN (static_cast<WifiInformationElementId>(48))
// 49 is reserved
#define IE_EXTENDED_SUPPORTED_RATES (static_cast<WifiInformationElementId>(50))
#define IE_AP_CHANNEL_REPORT (static_cast<WifiInformationElementId>(51))
#define IE_NEIGHBOR_REPORT (static_cast<WifiInformationElementId>(52))
#define IE_RCPI (static_cast<WifiInformationElementId>(53))
#define IE_MOBILITY_DOMAIN (static_cast<WifiInformationElementId>(54))
#define IE_FAST_BSS_TRANSITION (static_cast<WifiInformationElementId>(55))
#define IE_TIMEOUT_INTERVAL (static_cast<WifiInformationElementId>(56))
#define IE_RIC_DATA (static_cast<WifiInformationElementId>(57))
#define IE_DSE_REGISTERED_LOCATION (static_cast<WifiInformationElementId>(58))
#define IE_SUPPORTED_OPERATING_CLASSES (static_cast<WifiInformationElementId>(59))
#define IE_EXTENDED_CHANNEL_SWITCH_ANNOUNCEMENT (static_cast<WifiInformationElementId>(60))
#define IE_HT_OPERATION (static_cast<WifiInformationElementId>(61))
#define IE_SECONDARY_CHANNEL_OFFSET (static_cast<WifiInformationElementId>(62))
#define IE_BSS_AVERAGE_ACCESS_DELAY (static_cast<WifiInformationElementId>(63))
#define IE_ANTENNA (static_cast<WifiInformationElementId>(64))
#define IE_RSNI (static_cast<WifiInformationElementId>(65))
#define IE_MEASUREMENT_PILOT_TRANSMISSION (static_cast<WifiInformationElementId>(66))
#define IE_BSS_AVAILABLE_ADMISSION_CAPACITY (static_cast<WifiInformationElementId>(67))
#define IE_BSS_AC_ACCESS_DELAY (static_cast<WifiInformationElementId>(68))
#define IE_TIME_ADVERTISEMENT (static_cast<WifiInformationElementId>(69))
#define IE_RM_ENABLED_CAPACITIES (static_cast<WifiInformationElementId>(70))
#define IE_MULTIPLE_BSSID (static_cast<WifiInformationElementId>(71))
#define IE_20_40_BSS_COEXISTENCE (static_cast<WifiInformationElementId>(72))
#define IE_20_40_BSS_INTOLERANT_CHANNEL_REPORT (static_cast<WifiInformationElementId>(73))
#define IE_OVERLAPPING_BSS_SCAN_PARAMETERS (static_cast<WifiInformationElementId>(74))
#define IE_RIC_DESCRIPTOR (static_cast<WifiInformationElementId>(75))
#define IE_MANAGEMENT_MIC (static_cast<WifiInformationElementId>(76))
// 77 is reserved
#define IE_EVENT_REQUEST (static_cast<WifiInformationElementId>(78))
#define IE_EVENT_REPORT (static_cast<WifiInformationElementId>(79))
#define IE_DIAGNOSTIC_REQUEST (static_cast<WifiInformationElementId>(80))
#define IE_DIAGNOSTIC_REPORT (static_cast<WifiInformationElementId>(81))
#define IE_LOCATION_PARAMETERS (static_cast<WifiInformationElementId>(82))
#define IE_NONTRANSMITTED_BSSID_CAPABILITY (static_cast<WifiInformationElementId>(83))
#define IE_SSID_LIST (static_cast<WifiInformationElementId>(84))
#define IE_MULTIPLE_BSSID_INDEX (static_cast<WifiInformationElementId>(85))
#define IE_FMS_DESCRIPTOR (static_cast<WifiInformationElementId>(86))
#define IE_FMS_REQUEST (static_cast<WifiInformationElementId>(87))
#define IE_FMS_RESPONSE (static_cast<WifiInformationElementId>(88))
#define IE_QOS_TRAFFIC_CAPABILITY (static_cast<WifiInformationElementId>(89))
#define IE_BSS_MAX_IDLE_PERIOD (static_cast<WifiInformationElementId>(90))
#define IE_TFS_REQUEST (static_cast<WifiInformationElementId>(91))
#define IE_TFS_RESPONSE (static_cast<WifiInformationElementId>(92))
#define IE_WNM_SLEEP_MODE (static_cast<WifiInformationElementId>(93))
#define IE_TIM_BROADCAST_REQUEST (static_cast<WifiInformationElementId>(94))
#define IE_TIM_BROADCAST_RESPONSE (static_cast<WifiInformationElementId>(95))
#define IE_COLLOCATED_INTERFERENCE_REPORT (static_cast<WifiInformationElementId>(96))
#define IE_CHANNEL_USAGE (static_cast<WifiInformationElementId>(97))
#define IE_TIME_ZONE (static_cast<WifiInformationElementId>(98))
#define IE_DMS_REQUEST (static_cast<WifiInformationElementId>(99))
#define IE_DMS_RESPONSE (static_cast<WifiInformationElementId>(100))
#define IE_LINK_IDENTIFIER (static_cast<WifiInformationElementId>(101))
#define IE_WAKEUP_SCHEDULE (static_cast<WifiInformationElementId>(102))
// 103 is reserved
#define IE_CHANNEL_SWITCH_TIMING (static_cast<WifiInformationElementId>(104))
#define IE_PTI_CONTROL (static_cast<WifiInformationElementId>(105))
#define IE_TPU_BUFFER_STATUS (static_cast<WifiInformationElementId>(106))
#define IE_INTERWORKING (static_cast<WifiInformationElementId>(107))
#define IE_ADVERTISEMENT_PROTOCOL (static_cast<WifiInformationElementId>(108))
#define IE_EXPEDITED_BANDWIDTH_REQUEST (static_cast<WifiInformationElementId>(109))
#define IE_QOS_MAP_SET (static_cast<WifiInformationElementId>(110))
#define IE_ROAMING_CONSORTIUM (static_cast<WifiInformationElementId>(111))
#define IE_EMERGENCY_ALART_IDENTIFIER (static_cast<WifiInformationElementId>(112))
#define IE_MESH_CONFIGURATION (static_cast<WifiInformationElementId>(113))
#define IE_MESH_ID (static_cast<WifiInformationElementId>(114))
#define IE_MESH_LINK_METRIC_REPORT (static_cast<WifiInformationElementId>(115))
#define IE_CONGESTION_NOTIFICATION (static_cast<WifiInformationElementId>(116))
#define IE_MESH_PEERING_MANAGEMENT (static_cast<WifiInformationElementId>(117))
#define IE_MESH_CHANNEL_SWITCH_PARAMETERS (static_cast<WifiInformationElementId>(118))
#define IE_MESH_AWAKE_WINDOW (static_cast<WifiInformationElementId>(119))
#define IE_BEACON_TIMING (static_cast<WifiInformationElementId>(120))
#define IE_MCCAOP_SETUP_REQUEST (static_cast<WifiInformationElementId>(121))
#define IE_MCCAOP_SETUP_REPLY (static_cast<WifiInformationElementId>(122))
#define IE_MCCAOP_ADVERTISEMENT (static_cast<WifiInformationElementId>(123))
#define IE_MCCAOP_TEARDOWN (static_cast<WifiInformationElementId>(124))
#define IE_GANN (static_cast<WifiInformationElementId>(125))
#define IE_RANN (static_cast<WifiInformationElementId>(126))
// 67 to 126 are reserved
#define IE_EXTENDED_CAPABILITIES (static_cast<WifiInformationElementId>(127))
// 128 to 129 are reserved
#define IE_PREQ (static_cast<WifiInformationElementId>(130))
#define IE_PREP (static_cast<WifiInformationElementId>(131))
#define IE_PERR (static_cast<WifiInformationElementId>(132))
// 133 to 136 are reserved
#define IE_PROXY_UPDATE (static_cast<WifiInformationElementId>(137))
#define IE_PROXY_UPDATE_CONFIRMATION (static_cast<WifiInformationElementId>(138))
#define IE_AUTHENTICATED_MESH_PEERING_EXCHANGE (static_cast<WifiInformationElementId>(139))
#define IE_MIC (static_cast<WifiInformationElementId>(140))
#define IE_DESTINATION_URI (static_cast<WifiInformationElementId>(141))
#define IE_UAPSD_COEXISTENCE (static_cast<WifiInformationElementId>(142))
#define IE_DMG_WAKEUP_SCHEDULE (static_cast<WifiInformationElementId>(143))
#define IE_EXTENDED_SCHEDULE (static_cast<WifiInformationElementId>(144))
#define IE_STA_AVAILABILITY (static_cast<WifiInformationElementId>(145))
#define IE_DMG_TSPEC (static_cast<WifiInformationElementId>(146))
#define IE_NEXT_DMG_ATI (static_cast<WifiInformationElementId>(147))
#define IE_DMG_CAPABILITIES (static_cast<WifiInformationElementId>(148))
// 149 to 150 are reserved
#define IE_DMG_OPERATION (static_cast<WifiInformationElementId>(151))
#define IE_DMG_BSS_PARAMETER_CHANGE (static_cast<WifiInformationElementId>(152))
#define IE_DMG_BEAM_REFINEMENT (static_cast<WifiInformationElementId>(153))
#define IE_CHANNEL_MEASUREMENT_FEEDBACK (static_cast<WifiInformationElementId>(154))
// 155 to 156 are reserved
#define IE_AWAKE_WINDOW (static_cast<WifiInformationElementId>(157))
#define IE_MULTI_BAND (static_cast<WifiInformationElementId>(158))
#define IE_ADDBA_EXTENSION (static_cast<WifiInformationElementId>(159))
#define IE_NEXT_PCP_LIST (static_cast<WifiInformationElementId>(160))
#define IE_PCP_HANDOVER (static_cast<WifiInformationElementId>(161))
#define IE_DMG_LINK_MARGIN (static_cast<WifiInformationElementId>(162))
#define IE_SWITCHING_STREAM (static_cast<WifiInformationElementId>(163))
#define IE_SESSION_TRANSITION (static_cast<WifiInformationElementId>(164))
#define IE_DYNAMIC_TONE_PAIRING_REPORT (static_cast<WifiInformationElementId>(165))
#define IE_CLUSTER_REPORT (static_cast<WifiInformationElementId>(166))
#define IE_RELAY_CAPABILITIES (static_cast<WifiInformationElementId>(167))
#define IE_RELAY_TRANSFER_PARAMETER_SET (static_cast<WifiInformationElementId>(168))
#define IE_BEAMLINK_MAINTENANCE (static_cast<WifiInformationElementId>(169))
// 170 to 171 are reserved
#define IE_DMG_LINK_ADAPTATION_ACKNOWLEDGMENT (static_cast<WifiInformationElementId>(172))
// 173 is reserved
#define IE_MCCAOP_ADVERTISEMENT_OVERVIEW (static_cast<WifiInformationElementId>(174))
#define IE_QUIET_PERIOD_REQUEST (static_cast<WifiInformationElementId>(175))
// 176 is reserved
#define IE_QUIET_PERIOD_RESPONSE (static_cast<WifiInformationElementId>(177))
// 178 to 181 are reserved
#define IE_ECPAC_POLICY (static_cast<WifiInformationElementId>(182))
#define IE_CLUSTER_TIME_OFFSET (static_cast<WifiInformationElementId>(183))
#define IE_INTRA_ACCESS_CATEGORY_PRIORITY (static_cast<WifiInformationElementId>(184))
#define IE_SCS_DESCRIPTOR (static_cast<WifiInformationElementId>(185))
#define IE_QLOAD_REPORT (static_cast<WifiInformationElementId>(186))
#define IE_HCCA_TXOP_UPDATE_COUNT (static_cast<WifiInformationElementId>(187))
#define IE_HIGHER_LAYER_STREAM_ID (static_cast<WifiInformationElementId>(188))
#define IE_GCR_GROUP_ADDRESS (static_cast<WifiInformationElementId>(189))
#define IE_ANTENNA_SECTOR_ID_PATTERN (static_cast<WifiInformationElementId>(190))
#define IE_VHT_CAPABILITIES (static_cast<WifiInformationElementId>(191))
#define IE_VHT_OPERATION (static_cast<WifiInformationElementId>(192))
#define IE_EXTENDED_BSS_LOAD (static_cast<WifiInformationElementId>(193))
#define IE_WIDE_BANDWIDTH_CHANNEL_SWITCH (static_cast<WifiInformationElementId>(194))
#define IE_VHT_TRANSMIT_POWER_ENVELOPE (static_cast<WifiInformationElementId>(195))
#define IE_CHANNEL_SWITCH_WRAPPER (static_cast<WifiInformationElementId>(196))
#define IE_AID (static_cast<WifiInformationElementId>(197))
#define IE_QUIET_CHANNEL (static_cast<WifiInformationElementId>(198))
#define IE_OPERATING_MODE_NOTIFICATION (static_cast<WifiInformationElementId>(199))
#define IE_UPSIM (static_cast<WifiInformationElementId>(200))
#define IE_REDUCED_NEIGHBOR_REPORT (static_cast<WifiInformationElementId>(201))
// TODO Add 202 to 220. See Table 9-92 of 802.11-2020
#define IE_VENDOR_SPECIFIC (static_cast<WifiInformationElementId>(221))
// TODO Add 222 to 241. See Table 9-92 of 802.11-2020
#define IE_FRAGMENT (static_cast<WifiInformationElementId>(242))
// 243 to 254 are reserved
#define IE_EXTENSION (static_cast<WifiInformationElementId>(255))

#define IE_EXT_HE_CAPABILITIES (static_cast<WifiInformationElementId>(35))
#define IE_EXT_HE_OPERATION (static_cast<WifiInformationElementId>(36))
#define IE_EXT_UORA_PARAMETER_SET (static_cast<WifiInformationElementId>(37))
#define IE_EXT_MU_EDCA_PARAMETER_SET (static_cast<WifiInformationElementId>(38))

#define IE_EXT_NON_INHERITANCE (static_cast<WifiInformationElementId>(56))

#define IE_EXT_HE_6GHZ_CAPABILITIES (static_cast<WifiInformationElementId>(59))

#define IE_EXT_EHT_OPERATION (static_cast<WifiInformationElementId>(106))
#define IE_EXT_MULTI_LINK_ELEMENT (static_cast<WifiInformationElementId>(107))
#define IE_EXT_EHT_CAPABILITIES (static_cast<WifiInformationElementId>(108))
#define IE_EXT_TID_TO_LINK_MAPPING_ELEMENT (static_cast<WifiInformationElementId>(109))

/**
 * @brief Information element, as defined in 802.11-2007 standard
 * @ingroup wifi
 *
 * The IEEE 802.11 standard includes the notion of Information
 * Elements, which are encodings of management information to be
 * communicated between STAs in the payload of various frames of type
 * Management. Information Elements (IEs) have a common format, each
 * starting with a single octet - the Element ID, which indicates the
 * specific type of IE (a type to represent the options here is
 * defined as WifiInformationElementId). The next octet is a length field and
 * encodes the number of octets in the third and final field, which is
 * the IE Information field.
 *
 * The class ns3::WifiInformationElement provides a base for classes
 * which represent specific Information Elements. This class defines
 * pure virtual methods for serialisation
 * (ns3::WifiInformationElement::SerializeInformationField) and
 * deserialisation
 * (ns3::WifiInformationElement::DeserializeInformationField) of IEs, from
 * or to data members or other objects that simulation objects use to
 * maintain the relevant state.
 *
 * This class also provides an implementation of the equality
 * operator, which operates by comparing the serialized versions of
 * the two WifiInformationElement objects concerned.
 *
 * Elements are defined to have a common general format consisting of
 * a 1 octet Element ID field, a 1 octet length field, and a
 * variable-length element-specific information field. Each element is
 * assigned a unique Element ID as defined in this standard. The
 * Length field specifies the number of octets in the Information
 * field.
 *
 * Fragmentation of an Information Element is handled transparently by the base
 * class. Subclasses can simply serialize/deserialize their data into/from a
 * single large buffer. It is the base class that takes care of splitting
 * serialized data into multiple fragments (when serializing) or reconstructing
 * data from multiple fragments when deserializing.
 *
 * This class is pure virtual and acts as base for classes which know
 * how to serialize specific IEs.
 */
class WifiInformationElement : public SimpleRefCount<WifiInformationElement>
{
  public:
    virtual ~WifiInformationElement();
    /**
     * Serialize entire IE including Element ID and length fields. Handle
     * fragmentation of the IE if needed.
     *
     * @param i an iterator which points to where the IE should be written.
     *
     * @return an iterator
     */
    Buffer::Iterator Serialize(Buffer::Iterator i) const;
    /**
     * Deserialize entire IE (which may possibly be fragmented into multiple
     * elements), which must be present. The iterator passed in must be pointing
     * at the Element ID (i.e., the very first octet) of the correct type of
     * information element, otherwise this method will generate a fatal error.
     *
     * @param i an iterator which points to where the IE should be read.
     *
     * @return an iterator
     */
    Buffer::Iterator Deserialize(Buffer::Iterator i);
    /**
     * Deserialize entire IE (which may possibly be fragmented into multiple
     * elements) if it is present. The iterator passed in
     * must be pointing at the Element ID of an information element. If
     * the Element ID is not the one that the given class is interested
     * in then it will return the same iterator.
     *
     * @param i an iterator which points to where the IE should be read.
     *
     * @return an iterator
     */
    Buffer::Iterator DeserializeIfPresent(Buffer::Iterator i);
    /**
     * Get the size of the serialized IE including Element ID and
     * length fields (for every element this IE is possibly fragmented into).
     *
     * @return the size of the serialized IE in bytes
     */
    uint16_t GetSerializedSize() const;

    // Each subclass must implement these pure virtual functions:
    /**
     * Get the wifi information element ID
     * @returns the wifi information element ID
     */
    virtual WifiInformationElementId ElementId() const = 0;

    /**
     * Get the wifi information element ID extension
     * @returns the wifi information element ID extension
     */
    virtual WifiInformationElementId ElementIdExt() const;

    // In addition, a subclass may optionally override the following...
    /**
     * Generate human-readable form of IE
     *
     * @param os output stream
     */
    virtual void Print(std::ostream& os) const;
    /**
     * Compare two IEs for equality by ID & Length, and then through
     * memcmp of serialized version
     *
     * @param a another information element to compare with
     *
     * @return true if the two IEs are equal,
     *         false otherwise
     */
    virtual bool operator==(const WifiInformationElement& a) const;

  private:
    /**
     * Serialize an IE that needs to be fragmented.
     *
     * @param i an iterator which points to where the IE should be written.
     * @param size the size of the body of the IE
     * @return an iterator pointing to past the IE that was serialized
     */
    Buffer::Iterator SerializeFragments(Buffer::Iterator i, uint16_t size) const;
    /**
     * Deserialize the Information field of an IE. Also handle the case in which
     * the IE is fragmented.
     *
     * @param i an iterator which points to where the Information field should be read.
     * @param length the expected number of bytes to read
     * @return an iterator pointing to past the IE that was deserialized
     */
    Buffer::Iterator DoDeserialize(Buffer::Iterator i, uint16_t length);
    /**
     * Length of serialized information (i.e., the length of the body
     * of the IE, not including the Element ID and length octets. This
     * is the value that will appear in the second octet of the entire
     * IE - the length field - if the IE is not fragmented)
     *
     * @return the length of serialized information
     */
    virtual uint16_t GetInformationFieldSize() const = 0;
    /**
     * Serialize information (i.e., the body of the IE, not including
     * the Element ID and length octets)
     *
     * @param start an iterator which points to where the information should
     *        be written.
     */
    virtual void SerializeInformationField(Buffer::Iterator start) const = 0;
    /**
     * Deserialize information (i.e., the body of the IE, not including
     * the Element ID and length octets)
     *
     * @param start an iterator which points to where the information should be written.
     * @param length the expected number of bytes to read
     *
     * @return the number of bytes read
     */
    virtual uint16_t DeserializeInformationField(Buffer::Iterator start, uint16_t length) = 0;
};

/**
 * @brief Stream insertion operator.
 *
 * @param os the output stream
 * @param element the Information Element
 * @returns a reference to the stream
 */
std::ostream& operator<<(std::ostream& os, const WifiInformationElement& element);

} // namespace ns3

#endif /* WIFI_INFORMATION_ELEMENT_H */
