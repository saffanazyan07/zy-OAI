#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "fgs_service_request.h"
#include "fgmm_authentication_reject.h"
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

bool eq_auth_reject(fgmm_auth_reject_msg_t *a, fgmm_auth_reject_msg_t *b)
{
  _NAS_EQ_CHECK_INT(a->eap_msg.len, b->eap_msg.len);
  if (a->eap_msg.len > 0) {
    return !memcmp(a->eap_msg.msg, b->eap_msg.msg, a->eap_msg.len);
  }
  return true;
}

/**
 * @brief Test NAS Authentication Reject enc/dec
 */
static void test_auth_reject(void)
{
  // Dummy NAS Authentication Reject message
  uint8_t dummy_eap_msg[] = {0x12, 0x34, 0x56, 0x78, 0x91, 0x01, 0x23}; // Example EAP message
  fgmm_auth_reject_msg_t orig = {
      .eap_msg.len = sizeof(dummy_eap_msg),
  };
  memcpy(&orig.eap_msg.msg, dummy_eap_msg, orig.eap_msg.len);

  // Expected encoded data
  uint8_t dummy_enc[] = {
      IEI_EAPMSG, // IEI
      0x00, // length
      0x07, // length
      0x12, // EAP
      0x34, // EAP
      0x56, // EAP
      0x78, // EAP
      0x91, // EAP
      0x01, // EAP
      0x23  // EAP
  };

  // Buffer
  uint8_t buffer[64] = {0};

  // Encode
  int encoded_length = encode_fgmm_auth_reject(buffer, &orig, sizeof(buffer));
  AssertFatal(encoded_length >= 0, "encode_fgmm_auth_reject() failed\n");

  // Compare the raw encoded buffer with expected encoded data
  AssertFatal(encoded_length == sizeof(dummy_enc), "Encoded length mismatch!\n");
  AssertFatal(memcmp(buffer, dummy_enc, encoded_length) == 0, "Encoding mismatch!\n");

  // Decode
  fgmm_auth_reject_msg_t dec = {0};
  int decoded_length = decode_fgmm_auth_reject(&dec, buffer, encoded_length);
  AssertFatal(decoded_length >= 0, "decode_fgmm_auth_reject() failed\n");

  // Compare original and decoded messages
  AssertFatal(eq_auth_reject(&orig, &dec), "test_auth_reject() failed: original and decoded messages do not match\n");

}

int main()
{
  test_service_request();
  test_auth_reject();
  return 0;
}
