#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "fgs_service_request.h"
#include "fgmm_service_accept.h"
#include "fgmm_service_reject.h"
#include "nr_nas_msg.h"

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  printf("detected error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

/**
 * @brief Equality check for NAS Service Request enc/dec
 */
static bool eq_service_request(const fgs_service_request_msg_t *a, const fgs_service_request_msg_t *b)
{
  bool result = true;
  result &= memcmp(&a->naskeysetidentifier, &b->naskeysetidentifier, sizeof(NasKeySetIdentifier)) == 0;
  result &= a->serviceType == b->serviceType;
  result &= memcmp(&a->fiveg_s_tmsi, &b->fiveg_s_tmsi, sizeof(Stmsi5GSMobileIdentity_t)) == 0;
  return result;
}

/**
 * @brief Test NAS Service Request enc/dec
 */
static void test_service_request(void)
{
  // Dummy NAS Service Request message
  uint16_t amf_set_id = 0x48;
  uint16_t amf_pointer = 0x34;
  uint32_t tmsi = 0x56789ABC;
  fgs_service_request_msg_t original_msg = {
      .naskeysetidentifier = {.tsc = NAS_KEY_SET_IDENTIFIER_NATIVE, .naskeysetidentifier = NAS_KEY_SET_IDENTIFIER_NOT_AVAILABLE},
      .serviceType = SERVICE_TYPE_DATA,
      .fiveg_s_tmsi = {.spare = 0,
                       .typeofidentity = FGS_MOBILE_IDENTITY_5GS_TMSI,
                       .amfsetid = amf_set_id,
                       .amfpointer = amf_pointer,
                       .tmsi = tmsi}};

  uint8_t expected_encoded_data[] = {0x71,
                                     0x00,
                                     0x00,
                                     0xF4,
                                     (amf_set_id >> 2) & 0xFF,
                                     (amf_set_id << 6) | (amf_pointer & 0x3F),
                                     (tmsi >> 24) & 0xFF,
                                     (tmsi >> 16) & 0xFF,
                                     (tmsi >> 8) & 0xFF,
                                     tmsi & 0xFF};
  uint16_t tmp = htons(7); // length of 5GS mobile identity IE for 5G-S-TMSI (bytes)
  memcpy(expected_encoded_data + 1, &tmp, sizeof(tmp));

  // Buffer
  uint8_t buffer[64];
  memset(buffer, 0, sizeof(buffer));

  // Encode NAS Service Request
  int encoded_length = encode_fgs_service_request(buffer, &original_msg, sizeof(buffer));
  AssertFatal(encoded_length >= 0, "encode_fgs_service_request() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer, expected_encoded_data, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Request
  fgs_service_request_msg_t decoded_service_request = {0};
  int decoded_length = decode_fgs_service_request(&decoded_service_request, buffer, sizeof(buffer));
  AssertFatal(decoded_length >= 0, "decode_fgs_service_request() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_service_request(&original_msg, &decoded_service_request) == 0,
              "test_service_request() failed: original and decoded messages do not match\n");
}

/**
 * @brief Equality check for Service Accept (enc/dec)
 */
static bool eq_service_accept(const fgs_service_accept_msg_t *a, const fgs_service_accept_msg_t *b)
{
  for (int i = 0; i < a->num_psi_status; i++)
    _NAS_EQ_CHECK_INT(a->psi_status[i], b->psi_status[i]);
  for (int i = 0; i < a->num_psi_res; i++)
    _NAS_EQ_CHECK_INT(a->psi_res[i], b->psi_res[i]);
  _NAS_EQ_CHECK_INT(a->num_errors, b->num_errors);
  _NAS_EQ_CHECK_INT(*a->t3448_value, *b->t3448_value);
  return true;
}

/**
 * @brief Test NAS Service Accept enc/dec
 */
static void test_service_accept(void)
{
  // Dummy NAS Service Accept message
  fgs_service_accept_msg_t orig = {
      .num_psi_status = 3,
      .num_psi_res = 2,
      .num_errors = 1,
      .t3448_value = malloc(sizeof(*orig.t3448_value)),
  };
  orig.psi_status[0] = PDU_SESSION_ACTIVE;
  orig.psi_status[1] = PDU_SESSION_INACTIVE;
  orig.psi_status[2] = PDU_SESSION_ACTIVE;
  orig.psi_res[0] = REACTIVATION_FAILED;
  orig.psi_res[1] = REACTIVATION_SUCCESS;
  orig.cause[0].cause = Illegal_UE;
  orig.cause[0].pdu_session_id = 1;
  *orig.t3448_value = 100;

  // Expected encoded data
  uint8_t dummy_enc[] = {IEI_PDU_SESSION_STATUS, 0x02, 0x0a, 0x00,
                         IEI_PDU_SESSION_REACT_RESULT, 0x02, 0x02, 0x00,
                         IEI_PDU_SESSION_REACT_RESULT_ERROR_CAUSE, 0x00, 0x00, 0x01, 0x03,
                         IEI_T3448_VALUE, 0x01, 0x64};
  uint16_t len_status = htons(2);
  memcpy(dummy_enc + 9, &len_status, sizeof(len_status)); // length payload Reactivation Result Cause

  // Buffer
  uint8_t buffer[64];
  memset(buffer, 0, sizeof(buffer));

  // Encode NAS Service Accept
  int encoded_length = encode_fgs_service_accept(buffer, &orig, sizeof(buffer));
  AssertFatal(encoded_length >= 0, "encode_fgs_service_accept() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer, dummy_enc, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Accept
  fgs_service_accept_msg_t dec = {0};
  int decoded_length = decode_fgs_service_accept(&dec, buffer, encoded_length);
  AssertFatal(decoded_length >= 0, "decode_fgs_service_accept() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_service_accept(&orig, &dec), "test_service_accept() failed: original and decoded messages do not match\n");
  free_fgs_service_accept(&dec);
  free_fgs_service_accept(&orig);
}

/**
 * @brief Equality check for Service Reject (enc/dec)
 */
static bool eq_service_reject(const fgs_service_reject_msg_t *a, const fgs_service_reject_msg_t *b)
{
  _NAS_EQ_CHECK_INT(a->cause, b->cause);
  for (int i = 0; i < a->num_psi_status; i++)
    _NAS_EQ_CHECK_INT(a->psi_status[i], b->psi_status[i]);
  if (a->t3448_value && b->t3448_value)
    _NAS_EQ_CHECK_INT(*a->t3448_value, *b->t3448_value);
  if (a->t3446_value && b->t3446_value)
    _NAS_EQ_CHECK_INT(*a->t3446_value, *b->t3446_value);
  return true;
}

/**
 * @brief Test NAS Service Accept enc/dec
 */
static void test_service_reject(void)
{
  // Dummy NAS Service Accept message
  fgs_service_reject_msg_t orig = {
      .cause = Illegal_UE,
      .num_psi_status = 3,
      .t3446_value = malloc(sizeof(*orig.t3446_value)),
      .t3448_value = malloc(sizeof(*orig.t3448_value)),
  };
  orig.psi_status[0] = PDU_SESSION_ACTIVE;
  orig.psi_status[1] = PDU_SESSION_INACTIVE;
  orig.psi_status[2] = PDU_SESSION_ACTIVE;
  *orig.t3446_value = 100;
  *orig.t3448_value = 200;

  // Expected encoded data
  uint8_t dummy_enc[] = {0x03, 0x50, 0x02, 0x0a, 0x00, 0x5F, 0x01, 0x64, 0x6B, 0x01, 0xC8};

  // Buffer
  uint8_t buffer[64];
  memset(buffer, 0, sizeof(buffer));

  // Encode NAS Service Accept
  int encoded_length = encode_fgs_service_reject(&orig, buffer, sizeof(buffer));
  AssertFatal(encoded_length >= 0, "encode_fgs_service_reject() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(memcmp(buffer, dummy_enc, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode NAS Service Accept
  fgs_service_reject_msg_t dec = {0};
  int decoded_length = decode_fgs_service_reject(&dec, buffer, encoded_length);
  AssertFatal(decoded_length >= 0, "decode_fgs_service_reject() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_service_reject(&orig, &dec), "test_service_reject() failed: original and decoded messages do not match\n");
  free_fgs_service_reject(&dec);
  free_fgs_service_reject(&orig);
}

int main()
{
  test_service_request();
  test_service_accept();
  test_service_reject();
  return 0;
}
