/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#ifndef NR_NAS_DEFS_H
#define NR_NAS_DEFS_H

#include <stdint.h>
#include "openair3/UICC/usim_interface.h"
#include "fgs_nas_utils.h"
#include "FGSAuthenticationResponse.h"
#include "FGSDeregistrationRequestUEOriginating.h"
#include "FGSIdentityResponse.h"
#include "FGSMobileIdentity.h"
#include "FGSNASSecurityModeComplete.h"
#include "FGSUplinkNasTransport.h"
#include "RegistrationComplete.h"
#include "RegistrationRequest.h"
#include "fgs_service_request.h"

/* Message types defintions for:
   (a) 5GS Mobility Management (5GMM) (IDs 0x41 - 0x68)
       Table 9.7.1 of 3GPP TS 24.501
   (b) 5GS Session Management (5GSM) (IDs 0xc1 - 0xd6)
       Table 9.7.2 of 3GPP TS 24.501 */
#define FOREACH_TYPE(TYPE_DEF)                                         \
  TYPE_DEF(FGS_REGISTRATION_REQUEST, 0x41)                             \
  TYPE_DEF(FGS_REGISTRATION_ACCEPT, 0x42)                              \
  TYPE_DEF(FGS_REGISTRATION_COMPLETE, 0x43)                            \
  TYPE_DEF(FGS_REGISTRATION_REJECT, 0x44)                              \
  TYPE_DEF(FGS_DEREGISTRATION_REQUEST_UE_ORIGINATING, 0x45)            \
  TYPE_DEF(FGS_DEREGISTRATION_ACCEPT_UE_ORIGINATING, 0x46)             \
  TYPE_DEF(FGS_DEREGISTRATION_REQUEST_UE_TERMINATED, 0x47)             \
  TYPE_DEF(FGS_DEREGISTRATION_ACCEPT_UE_TERMINATED, 0x48)              \
  TYPE_DEF(FGS_SERVICE_REQUEST, 0x4c)                                  \
  TYPE_DEF(FGS_SERVICE_REJECT, 0x4d)                                   \
  TYPE_DEF(FGS_SERVICE_ACCEPT, 0x4e)                                   \
  TYPE_DEF(FGS_CONTROL_PLANE_SERVICE_REQUEST, 0x4f)                    \
  TYPE_DEF(FGS_NSS_AUTHENTICATION_COMMAND, 0x50)                       \
  TYPE_DEF(FGS_NSS_AUTHENTICATION_COMPLETE, 0x51)                      \
  TYPE_DEF(FGS_NSS_AUTHENTICATION_RESULT, 0x52)                        \
  TYPE_DEF(FGS_CONFIGURATION_UPDATE_COMMAND, 0x54)                     \
  TYPE_DEF(FGS_CONFIGURATION_UPDATE_COMPLETE, 0x55)                    \
  TYPE_DEF(FGS_AUTHENTICATION_REQUEST, 0x56)                           \
  TYPE_DEF(FGS_AUTHENTICATION_RESPONSE, 0x57)                          \
  TYPE_DEF(FGS_AUTHENTICATION_REJECT, 0x58)                            \
  TYPE_DEF(FGS_AUTHENTICATION_FAILURE, 0x59)                           \
  TYPE_DEF(FGS_AUTHENTICATION_RESULT, 0x5a)                            \
  TYPE_DEF(FGS_IDENTITY_REQUEST, 0x5b)                                 \
  TYPE_DEF(FGS_IDENTITY_RESPONSE, 0x5c)                                \
  TYPE_DEF(FGS_SECURITY_MODE_COMMAND, 0x5d)                            \
  TYPE_DEF(FGS_SECURITY_MODE_COMPLETE, 0x5e)                           \
  TYPE_DEF(FGS_SECURITY_MODE_REJECT, 0x5f)                             \
  TYPE_DEF(FGS_5GMM_STATUS, 0x64)                                      \
  TYPE_DEF(FGS_NOTIFICATION, 0x65)                                     \
  TYPE_DEF(FGS_NOTIFICATION_RESPONSE, 0x66)                            \
  TYPE_DEF(FGS_UPLINK_NAS_TRANSPORT, 0x67)                             \
  TYPE_DEF(FGS_DOWNLINK_NAS_TRANSPORT, 0x68)                           \
  TYPE_DEF(FGS_PDU_SESSION_ESTABLISHMENT_REQ, 0xc1)                    \
  TYPE_DEF(FGS_PDU_SESSION_ESTABLISHMENT_ACC, 0xc2)                    \
  TYPE_DEF(FGS_PDU_SESSION_ESTABLISHMENT_REJ, 0xc3)                    \
  TYPE_DEF(FGS_PDU_SESSION_AUTH_COMMAND, 0xc5)                         \
  TYPE_DEF(FGS_PDU_SESSION_AUTH_COMPLETE, 0xc6)                        \
  TYPE_DEF(FGS_PDU_SESSION_AUTH_RESULT, 0xc7)                          \
  TYPE_DEF(FGS_PDU_SESSION_MODIFICATION_REQ, 0xc9)                     \
  TYPE_DEF(FGS_PDU_SESSION_MODIFICATION_REJ, 0xca)                     \
  TYPE_DEF(FGS_PDU_SESSION_MODIFICATION_COMMAND, 0xcb)                 \
  TYPE_DEF(FGS_PDU_SESSION_MODIFICATION_COMPLETE, 0xcc)                \
  TYPE_DEF(FGS_PDU_SESSION_MODIFICATION_COMMAND_REJ, 0xcd)             \
  TYPE_DEF(FGS_PDU_SESSION_RELEASE_REQ, 0xd1)                          \
  TYPE_DEF(FGS_PDU_SESSION_RELEASE_REJ, 0xd2)                          \
  TYPE_DEF(FGS_PDU_SESSION_RELEASE_COMMAND, 0xd3)                      \
  TYPE_DEF(FGS_PDU_SESSION_RELEASE_COMPLETE, 0xd4)                     \
  TYPE_DEF(FGS_5GSM_STATUS, 0xd6)

static const text_info_t message_text_info[] = {
  FOREACH_TYPE(TO_TEXT)
};

//! Tasks id of each task
typedef enum {
  FOREACH_TYPE(TO_ENUM)
} fgs_nas_msg_t;

// TS 24.501
#define FOREACH_HEADER(TYPE_DEF)                     \
  TYPE_DEF(PLAIN_5GS_MSG, 0)                         \
  TYPE_DEF(INTEGRITY_PROTECTED, 1)                   \
  TYPE_DEF(INTEGRITY_PROTECTED_AND_CIPHERED, 2)      \
  TYPE_DEF(INTEGRITY_PROTECTED_WITH_NEW_SECU_CTX, 3) \
  TYPE_DEF(INTEGRITY_PROTECTED_AND_CIPHERED_WITH_NEW_SECU_CTX, 4)

typedef enum { FOREACH_HEADER(TO_ENUM) } Security_header_t;

static const text_info_t security_header_type_s[] = {FOREACH_HEADER(TO_TEXT)};

// table  9.11.3.2.1
#define FOREACH_CAUSE(CAUSE_DEF) \
  CAUSE_DEF(Illegal_UE,0x3 )\
  CAUSE_DEF(PEI_not_accepted,0x5 )\
  CAUSE_DEF(Illegal_ME,0x6 )\
  CAUSE_DEF(SGS_services_not_allowed,0x7 )\
  CAUSE_DEF(UE_identity_cannot_be_derived_by_the_network,0x9 )\
  CAUSE_DEF(Implicitly_de_registered,0x0a )\
  CAUSE_DEF(PLMN_not_allowed,0x0b )\
  CAUSE_DEF(Tracking_area_not_allowed,0x0c )\
  CAUSE_DEF(Roaming_not_allowed_in_this_tracking_area,0x0d )\
  CAUSE_DEF(No_suitable_cells_in_tracking_area,0x0f )\
  CAUSE_DEF(MAC_failure,0x14 )\
  CAUSE_DEF(Synch_failure,0x15 )\
  CAUSE_DEF(Congestion,0x16 )\
  CAUSE_DEF(UE_security_capabilities_mismatch,0x17 )\
  CAUSE_DEF(Security_mode_rejected_unspecified,0x18 )\
  CAUSE_DEF(Non_5G_authentication_unacceptable,0x1a )\
  CAUSE_DEF(N1_mode_not_allowed,0x1b )\
  CAUSE_DEF(Restricted_service_area,0x1c )\
  CAUSE_DEF(Redirection_to_EPC_required,0x1f )\
  CAUSE_DEF(LADN_not_available,0x2b )\
  CAUSE_DEF(No_network_slices_available,0x3e )\
  CAUSE_DEF(Maximum_number_of_PDU_sessions_reached,0x41 )\
  CAUSE_DEF(Insufficient_resources_for_specific_slice_and_DNN,0x43 )\
  CAUSE_DEF(Insufficient_resources_for_specific_slice,0x45 )\
  CAUSE_DEF(ngKSI_already_in_use,0x47 )\
  CAUSE_DEF(Non_3GPP_access_to_5GCN_not_allowed,0x48 )\
  CAUSE_DEF(Serving_network_not_authorized,0x49 )\
  CAUSE_DEF(Temporarily_not_authorized_for_this_SNPN,0x4A )\
  CAUSE_DEF(Permanently_not_authorized_for_this_SNPN,0x4b )\
  CAUSE_DEF(Not_authorized_for_this_CAG_or_authorized_for_CAG_cells_only,0x4c )\
  CAUSE_DEF(Wireline_access_area_not_allowed,0x4d )\
  CAUSE_DEF(Payload_was_not_forwarded,0x5a )\
  CAUSE_DEF(DNN_not_supported_or_not_subscribed_in_the_slice,0x5b )\
  CAUSE_DEF(Insufficient_user_plane_resources_for_the_PDU_session,0x5c )\
  CAUSE_DEF(Semantically_incorrect_message,0x5f )\
  CAUSE_DEF(Invalid_mandatory_information,0x60 )\
  CAUSE_DEF(Message_type_non_existent_or_not_implemented,0x61 )\
  CAUSE_DEF(Message_type_not_compatible_with_the_protocol_state,0x62 )\
  CAUSE_DEF(Information_element_non_existent_or_not_implemented,0x63 )\
  CAUSE_DEF(Conditional_IE_error,0x64 )\
  CAUSE_DEF(Message_not_compatible_with_the_protocol_state,0x65 )\
  CAUSE_DEF(Protocol_error_unspecified,0x67 )

/* Map task id to printable name. */
#define CAUSE_TEXT(LabEl, nUmID) {nUmID, #LabEl},

static const text_info_t cause_text_info[] = {
  FOREACH_CAUSE(TO_TEXT)
};

#define CAUSE_ENUM(LabEl, nUmID ) LabEl = nUmID,
//! Tasks id of each task
typedef enum {
  FOREACH_CAUSE(TO_ENUM)
} cause_id_t;

//_table_9.11.4.2.1:_5GSM_cause_information_element
#define FOREACH_CAUSE_SECU(CAUSE_SECU_DEF) \
  CAUSE_SECU_DEF(Security_Operator_determined_barring,0x08 )\
  CAUSE_SECU_DEF(Security_Insufficient_resources,0x1a )\
  CAUSE_SECU_DEF(Security_Missing_or_unknown_DNN,0x1b )\
  CAUSE_SECU_DEF(Security_Unknown_PDU_session_type,0x1c )\
  CAUSE_SECU_DEF(Security_User_authentication_or_authorization_failed,0x1d )\
  CAUSE_SECU_DEF(Security_Request_rejected_unspecified,0x1f )\
  CAUSE_SECU_DEF(Security_Service_option_not_supported,0x20 )\
  CAUSE_SECU_DEF(Security_Requested_service_option_not_subscribed,0x21 )\
  CAUSE_SECU_DEF(Security_Service_option_temporarily_out_of_order,0x22 )\
  CAUSE_SECU_DEF(Security_PTI_already_in_use,0x23 )\
  CAUSE_SECU_DEF(Security_Regular_deactivation,0x24 )\
  CAUSE_SECU_DEF(Security_Network_failure,0x26 )\
  CAUSE_SECU_DEF(Security_Reactivation_requested,0x27 )\
  CAUSE_SECU_DEF(Security_Semantic_error_in_the_TFT_operation,0x29 )\
  CAUSE_SECU_DEF(Security_Syntactical_error_in_the_TFT_operation,0x2a )\
  CAUSE_SECU_DEF(Security_Invalid_PDU_session_identity,0x2b )\
  CAUSE_SECU_DEF(Security_Semantic_errors_in_packet_filter,0x2c )\
  CAUSE_SECU_DEF(Security_Syntactical_error_in_packet_filter,0x2d )\
  CAUSE_SECU_DEF(Security_Out_of_LADN_service_area,0x2e )\
  CAUSE_SECU_DEF(Security_PTI_mismatch,0x2f )\
  CAUSE_SECU_DEF(Security_PDU_session_type_IPv4_only_allowed,0x32 )\
  CAUSE_SECU_DEF(Security_PDU_session_type_IPv6_only_allowed,0x33 )\
  CAUSE_SECU_DEF(Security_PDU_session_does_not_exist,0x36 )\
  CAUSE_SECU_DEF(Security_PDU_session_type_IPv4v6_only_allowed,0x39 )\
  CAUSE_SECU_DEF(Security_PDU_session_type_Unstructured_only_allowed,0x3a )\
  CAUSE_SECU_DEF(Security_PDU_session_type_Ethernet_only_allowed,0x3d )\
  CAUSE_SECU_DEF(Security_Insufficient_resources_for_specific_slice_and_DNN,0x43 )\
  CAUSE_SECU_DEF(Security_Not_supported_SSC_mode,0x44 )\
  CAUSE_SECU_DEF(Security_Insufficient_resources_for_specific_slice,0x45 )\
  CAUSE_SECU_DEF(Security_Missing_or_unknown_DNN_in_a_slice,0x46 )\
  CAUSE_SECU_DEF(Security_Invalid_PTI_value,0x51 )\
  CAUSE_SECU_DEF(Security_Maximum_data_rate_per_UE_for_user_plane_integrity_protection_is_too_low,0x52 )\
  CAUSE_SECU_DEF(Security_Semantic_error_in_the_QoS_operation,0x53 )\
  CAUSE_SECU_DEF(Security_Syntactical_error_in_the_QoS_operation,0x54 )\
  CAUSE_SECU_DEF(Security_Invalid_mapped_EPS_bearer_identity,0x55 )\
  CAUSE_SECU_DEF(Security_Semantically_incorrect_message,0x5f )\
  CAUSE_SECU_DEF(Security_Invalid_mandatory_information,0x60 )\
  CAUSE_SECU_DEF(Security_Message_type_non_existent_or_not_implemented,0x61 )\
  CAUSE_SECU_DEF(Security_Message_type_not_compatible_with_the_protocol_state,0x62 )\
  CAUSE_SECU_DEF(Security_Information_element_non_existent_or_not_implemented,0x63 )\
  CAUSE_SECU_DEF(Security_Conditional_IE_error,0x64 )\
  CAUSE_SECU_DEF(Security_Message_not_compatible_with_the_protocol_state,0x65 )\
  CAUSE_SECU_DEF(Security_Protocol_error_unspecified,0x6f )

static const text_info_t cause_secu_text_info[] = {
  FOREACH_CAUSE_SECU(TO_TEXT)
};

//! Tasks id of each task
typedef enum {
  FOREACH_CAUSE_SECU(TO_ENUM)
} cause_secu_id_t;
								
/**
 * Security protected 5GS NAS message header (9.1.1 of 3GPP TS 24.501)
 * either 5GMM or 5GSM
 * Standard L3 message header (11.2.3.1 of 3GPP TS 24.007)
 */
typedef struct {
  // Extended Protocol Discriminator
  uint8_t protocol_discriminator;
  // Security Header Type
  uint8_t security_header_type;
  // Message Authentication Code
  uint32_t message_authentication_code;
  // Sequence Number
  uint8_t sequence_number;
} __attribute__((packed)) fgs_nas_message_security_header_t;

/// 5GS Protocol Discriminator identifiers

typedef enum fgs_protocol_discriminator_e {
  // 5GS Mobility Management
  FGS_MOBILITY_MANAGEMENT_MESSAGE = 0x7E,
  // 5GS Session Management
  FGS_SESSION_MANAGEMENT_MESSAGE = 0x2E,
} fgs_protocol_discriminator_t;

/// 5GMM - 5GS mobility management

/**
 * Header of a plain 5GMM NAS message (5GS)
 * Standard L3 message header (11.2.3.1 of 3GPP TS 24.007)
 */
typedef struct {
  uint8_t ex_protocol_discriminator;
  uint8_t security_header_type;
  uint8_t message_type;
} fgmm_msg_header_t;

/* Plain 5GMM NAS message (5GS) */
typedef struct {
  fgmm_msg_header_t header;
  union {
    registration_request_msg registration_request;
    fgs_service_request_msg_t service_request;
    fgmm_identity_response_msg fgs_identity_response;
    fgs_authentication_response_msg fgs_auth_response;
    fgs_deregistration_request_ue_originating_msg fgs_deregistration_request_ue_originating;
    fgs_security_mode_complete_msg fgs_security_mode_complete;
    registration_complete_msg registration_complete;
    fgs_uplink_nas_transport_msg uplink_nas_transport;
  } mm_msg; /* 5GS Mobility Management messages */
} fgmm_nas_message_plain_t;

/* Security protected 5GMM NAS message (5GS) */
typedef struct {
  fgs_nas_message_security_header_t header;
  fgmm_nas_message_plain_t plain;
} fgmm_nas_msg_security_protected_t;

/// 5GSM - 5GS session management

/**
 * Header of a plain 5GSM NAS message (5GS)
 * Standard L3 message header (11.2.3.1 of 3GPP TS 24.007)
 */
typedef struct {
  // Extended protocol discriminator
  uint8_t ex_protocol_discriminator;
  // PDU session identity
  uint8_t pdu_session_id;
  // Procedure transaction identity
  uint8_t pti;
  // Message type
  uint8_t message_type;
} fgsm_msg_header_t;

/// Function prototypes

int mm_msg_encode(const fgmm_nas_message_plain_t *mm_msg, uint8_t *buffer, uint32_t len);
int nas_protected_security_header_encode(uint8_t *buffer, const fgs_nas_message_security_header_t *header, int length);
int _nas_mm_msg_encode_header(const fgmm_msg_header_t *header, uint8_t *buffer, uint32_t len);
uint8_t decode_5gmm_msg_header(fgmm_msg_header_t *mm_header, const uint8_t *buffer, uint32_t len);
uint8_t decode_5gsm_msg_header(fgsm_msg_header_t *sm_header, const uint8_t *buffer, uint32_t len);
int decode_5gs_security_protected_header(fgs_nas_message_security_header_t *header, const uint8_t *buf, uint32_t len);

#endif // NR_NAS_DEFS_H
