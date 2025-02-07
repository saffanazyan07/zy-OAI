#include <map>
using namespace std;

#ifdef __cplusplus
extern "C" {
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>

#include "common/platform_types.h"
#include <openair3/UTILS/conversions.h>
#include "common/utils/LOG/log.h"
#include <common/utils/ocp_itti/intertask_interface.h>
#include <openair2/COMMON/gtpv1_u_messages_types.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <openair2/LAYER2/PDCP_v10.1.0/pdcp.h>
#include <openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h>
#include <openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h>

#include "openair2/SDAP/nr_sdap/nr_sdap.h"
#include "sim.h"

// library for PPPoE
#include <linux/if_packet.h>
#include <linux/if_pppox.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <signal.h>
//#include <syslog.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>


#if defined(HAVE_LINUX_IF_H)
#include <linux/if.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include <rp-pppoe-master/src/relay.h>

#pragma pack(1)

typedef struct Gtpv1uMsgHeader {
  uint8_t PN:1;
  uint8_t S:1;
  uint8_t E:1;
  uint8_t spare:1;
  uint8_t PT:1;
  uint8_t version:3;
  uint8_t msgType;
  uint16_t msgLength;
  teid_t teid;
} __attribute__((packed)) Gtpv1uMsgHeaderT;

//TS 38.425, Figure 5.5.2.2-1
typedef struct DlDataDeliveryStatus_flags {
  uint8_t LPR:1;                    //Lost packet report
  uint8_t FFI:1;                    //Final Frame Ind
  uint8_t deliveredPdcpSn:1;        //Highest Delivered NR PDCP SN Ind
  uint8_t transmittedPdcpSn:1;      //Highest Transmitted NR PDCP SN Ind
  uint8_t pduType:4;                //PDU type
  uint8_t CR:1;                     //Cause Report
  uint8_t deliveredReTxPdcpSn:1;    //Delivered retransmitted NR PDCP SN Ind
  uint8_t reTxPdcpSn:1;             //Retransmitted NR PDCP SN Ind
  uint8_t DRI:1;                    //Data Rate Indication
  uint8_t deliveredPdcpSnRange:1;   //Delivered NR PDCP SN Range Ind
  uint8_t spare:3;
  uint32_t drbBufferSize;            //Desired buffer size for the data radio bearer
} __attribute__((packed)) DlDataDeliveryStatus_flagsT;

typedef struct Gtpv1uMsgHeaderOptFields {
  uint8_t seqNum1Oct;
  uint8_t seqNum2Oct;
  uint8_t NPDUNum;
  uint8_t NextExtHeaderType;    
} __attribute__((packed)) Gtpv1uMsgHeaderOptFieldsT;

#define DL_PDU_SESSION_INFORMATION 0
#define UL_PDU_SESSION_INFORMATION 1

  typedef struct PDUSessionContainer {
  uint8_t spare:4;
  uint8_t PDU_type:4;
  uint8_t QFI:6;
  uint8_t Reflective_QoS_activation:1;
  uint8_t Paging_Policy_Indicator:1;
} __attribute__((packed)) PDUSessionContainerT;

typedef struct Gtpv1uExtHeader {
  uint8_t ExtHeaderLen;
  PDUSessionContainerT pdusession_cntr;
  uint8_t NextExtHeaderType;
}__attribute__((packed)) Gtpv1uExtHeaderT;

#pragma pack()

// TS 29.281, fig 5.2.1-3
#define PDU_SESSION_CONTAINER       (0x85)
#define NR_RAN_CONTAINER            (0x84)

// TS 29.281, 5.2.1
#define EXT_HDR_LNTH_OCTET_UNITS    (4)
#define NO_MORE_EXT_HDRS            (0)

// TS 29.060, table 7.1 defines the possible message types
// here are all the possible messages (3GPP R16)
#define GTP_ECHO_REQ                                         (1)
#define GTP_ECHO_RSP                                         (2)
#define GTP_ERROR_INDICATION                                 (26)
#define GTP_SUPPORTED_EXTENSION_HEADER_INDICATION            (31)
#define GTP_END_MARKER                                       (254)
#define GTP_GPDU                                             (255)

typedef struct gtpv1u_bearer_s {
  /* TEID used in dl and ul */
  teid_t          teid_incoming;                ///< eNB TEID
  teid_t          teid_outgoing;                ///< Remote TEID
  in_addr_t       outgoing_ip_addr;
  struct in6_addr outgoing_ip6_addr;
  tcp_udp_port_t  outgoing_port;
  uint16_t        seqNum;
  uint8_t         npduNum;
  int outgoing_qfi;
} gtpv1u_bearer_t;

typedef struct {
  map<ue_id_t, gtpv1u_bearer_t> bearers;
  teid_t outgoing_teid;
} teidData_t;

typedef struct {
  ue_id_t ue_id;
  ebi_t incoming_rb_id;
  gtpCallback callBack;
  teid_t outgoing_teid;
  gtpCallbackSDAP callBackSDAP;
  int pdusession_id;
} ueidData_t;

class gtpEndPoint {
 public:
  openAddr_t addr;
  uint8_t foundAddr[20];
  int foundAddrLen;
  int ipVersion;
  map<uint64_t, teidData_t> ue2te_mapping;
  // we use the same port number for source and destination address
  // this allow using non standard gtp port number (different from 2152)
  // and so, for example tu run 4G and 5G cores on one system
  tcp_udp_port_t get_dstport() {
    return (tcp_udp_port_t)atol(addr.destinationService);
  }
};

class gtpEndPoints {
 public:
  pthread_mutex_t gtp_lock=PTHREAD_MUTEX_INITIALIZER;
  // the instance id will be the Linux socket handler, as this is uniq
  map<uint64_t, gtpEndPoint> instances;
  map<uint64_t, ueidData_t> te2ue_mapping;
  gtpEndPoints() {
    unsigned int seed;
    fill_random(&seed, sizeof(seed));
    srandom(seed);
  }

  ~gtpEndPoints() {
    // automatically close all sockets on quit
    for (const auto &p : instances)
      close(p.first);
  }
};

static gtpEndPoints globGtp;

// note TEid 0 is reserved for specific usage: echo req/resp, error and supported extensions
static  teid_t gtpv1uNewTeid(void) {
#ifdef GTPV1U_LINEAR_TEID_ALLOCATION
  g_gtpv1u_teid = g_gtpv1u_teid + 1;
  return g_gtpv1u_teid;
#else
  return random() + random() % (RAND_MAX - 1) + 1;
#endif
}

instance_t legacyInstanceMapping=0;
#define compatInst(a) ((a)==0 || (a)==INSTANCE_DEFAULT ? legacyInstanceMapping:a)

#define getInstRetVoid(insT)                                    \
  auto instChk=globGtp.instances.find(compatInst(insT));    \
  if (instChk == globGtp.instances.end()) {                        \
    LOG_E(GTPU,"try to get a gtp-u not existing output\n");     \
    pthread_mutex_unlock(&globGtp.gtp_lock);                    \
    return;                                                     \
  }                                                             \
  gtpEndPoint * inst=&instChk->second;
  
#define getInstRetInt(insT)                                    \
  auto instChk=globGtp.instances.find(compatInst(insT));    \
  if (instChk == globGtp.instances.end()) {                        \
    LOG_E(GTPU,"try to get a gtp-u not existing output\n");     \
    pthread_mutex_unlock(&globGtp.gtp_lock);                    \
    return GTPNOK;                                                     \
  }                                                             \
  gtpEndPoint * inst=&instChk->second;

//edited by zyzy

/*
#define getUeRetVoid(insT, Ue)                                            \
    auto ptrUe=insT->ue2te_mapping.find(Ue);                        \
                                                                        \
  if (  ptrUe==insT->ue2te_mapping.end() ) {                          \
    LOG_E(GTPU, "[%ld] %s failed: while getting ue id %ld in hashtable ue_mapping\n", instance, __func__, ue_id); \
    pthread_mutex_unlock(&globGtp.gtp_lock);                            \
    return;                                                             \
  }
  
#define getUeRetInt(insT, Ue)                                            \
    auto ptrUe=insT->ue2te_mapping.find(Ue);                        \
                                                                        \
  if (  ptrUe==insT->ue2te_mapping.end() ) {                          \
    LOG_E(GTPU, "[%ld] %s failed: while getting ue id %ld in hashtable ue_mapping\n", instance, __func__, ue_id); \
    pthread_mutex_unlock(&globGtp.gtp_lock);                            \
    return GTPNOK;                                                             \
  }
  */

 #define getUeRetVoid(insT, Ue)                                            \
    auto ptrUe=insT->ue2te_mapping.find(Ue);                        \
                                                                        \
    if (ptrUe == insT->ue2te_mapping.end()) {                          \
        LOG_E(GTPU, "[%ld] %s failed: while getting ue id %ld in hashtable ue_mapping\n", instance, __func__, Ue); \
        pthread_mutex_unlock(&globGtp.gtp_lock);                            \
        return;                                                             \
    } else { \
        LOG_I(GTPU, "[%ld] %s success: UE ID %ld\n", instance, __func__, Ue); \
    }

#define getUeRetInt(insT, Ue)                                            \
    auto ptrUe=insT->ue2te_mapping.find(Ue);                        \
                                                                        \
    if (ptrUe == insT->ue2te_mapping.end()) {                          \
        LOG_E(GTPU, "[%ld] %s failed: while getting ue id %ld in hashtable ue_mapping\n", instance, __func__, Ue); \
        pthread_mutex_unlock(&globGtp.gtp_lock);                            \
        return GTPNOK;                                                             \
    } else { \
        LOG_I(GTPU, "[%ld] %s success: UE ID %ld\n", instance, __func__, Ue); \
    }
///////////////////////////////////
/////////edited by zyzy////////////
///////////////////////////////////
/*
void sendPADO(PPPoEPacket *padi) {
    PPPoEPacket pado;
    memset(&pado, 0, sizeof(PPPoEPacket));

    pado.code = CODE_PADO;
    pado.session = htons(0);
    pado.length = htons(0);
    memcpy(pado.ethHdr.h_dest, padi->ethHdr.h_source, ETH_ALEN);
    memcpy(pado.ethHdr.h_source, my_mac, ETH_ALEN);
    pado.ethHdr.h_proto = htons(ETH_PPPOE_DISCOVERY);

    LOG_I(GTPU, "Sending PADO response\n");
    sendPacket(NULL, discoverySock, &pado, sizeof(pado));
}

void sendPADS(PPPoEPacket *padr) {
    PPPoEPacket pads;
    memset(&pads, 0, sizeof(PPPoEPacket));

    pads.code = CODE_PADS;
    pads.session = htons(newPPPoESession());
    pads.length = htons(0);
    memcpy(pads.ethHdr.h_dest, padr->ethHdr.h_source, ETH_ALEN);
    memcpy(pads.ethHdr.h_source, my_mac, ETH_ALEN);
    pads.ethHdr.h_proto = htons(ETH_PPPOE_DISCOVERY);

    LOG_I(GTPU, "Sending PADS response, session created\n");
    sendPacket(NULL, discoverySock, &pads, sizeof(pads));
}

void closePPPoESession(PPPoEPacket *padt) {
    uint16_t session = ntohs(padt->session);
    LOG_I(GTPU, "Closing PPPoE session: %d\n", session);
    removePPPoESession(session);
}

void gtpv1uSendPPPoEPacket(instance_t instance,
                           ue_id_t ue_id,
                           int bearer_id,
                           PPPoEPacket *pkt,
                           size_t len) {
    pthread_mutex_lock(&globGtp.gtp_lock);
    getInstRetVoid(compatInst(instance));
    getUeRetVoid(inst, ue_id);

    auto ptr2 = ptrUe->second.bearers.find(bearer_id);
    if (ptr2 == ptrUe->second.bearers.end()) {
        LOG_E(GTPU, "[%ld] GTP-U instance: No UE session found for PPPoE packet\n", instance);
        pthread_mutex_unlock(&globGtp.gtp_lock);
        return;
    }

    LOG_I(GTPU, "[%ld] Sending PPPoE packet over GTP-U tunnel for UE:%lu, Bearer:%d\n", instance, ue_id, bearer_id);
    
    gtpv1uCreateAndSendMsg(compatInst(instance),
                           ptr2->second.outgoing_ip_addr,
                           ptr2->second.outgoing_port,
                           GTP_GPDU,
                           ptr2->second.teid_outgoing,
                           (uint8_t *)pkt,
                           len,
                           false,
                           false,
                           0,
                           0,
                           NO_MORE_EXT_HDRS,
                           NULL,
                           0);
}
*/
/////////////////////////////////////
//edited by zyzy end

#define HDR_MAX 256 // 256 is supposed to be larger than any gtp header
static int gtpv1uCreateAndSendMsg(int h,
                                  uint32_t peerIp,
                                  uint16_t peerPort,
                                  int msgType,
                                  teid_t teid,
                                  uint8_t *Msg,
                                  int msgLen,
                                  bool seqNumFlag,
                                  bool npduNumFlag,
                                  int seqNum,
                                  int npduNum,
                                  int extHdrType,
                                  uint8_t *extensionHeader_buffer,
                                  uint8_t extensionHeader_length) {
  LOG_D(GTPU, "Peer IP:%u peer port:%u outgoing teid:%u \n", peerIp, peerPort, teid);

  uint8_t buffer[msgLen+HDR_MAX]; 
  uint8_t *curPtr=buffer;
  Gtpv1uMsgHeaderT      *msgHdr = (Gtpv1uMsgHeaderT *)buffer ;
  // N should be 0 for us (it was used only in 2G and 3G)
  msgHdr->PN=npduNumFlag;
  msgHdr->S=seqNumFlag;
  msgHdr->E = extHdrType != NO_MORE_EXT_HDRS;
  msgHdr->spare=0;
  //PT=0 is for GTP' TS 32.295 (charging)
  msgHdr->PT=1;
  msgHdr->version=1;
  msgHdr->msgType=msgType;
  msgHdr->teid=htonl(teid);

  curPtr+=sizeof(Gtpv1uMsgHeaderT);

  if (seqNumFlag || (extHdrType != NO_MORE_EXT_HDRS) || npduNumFlag) {
    *(uint16_t *)curPtr = seqNumFlag ? seqNum : 0x0000;
    curPtr+=sizeof(uint16_t);
    *(uint8_t *)curPtr = npduNumFlag ? npduNum : 0x00;
    curPtr++;
    *(uint8_t *)curPtr = extHdrType;
    curPtr++;
  }

  // Bug: if there is more than one extension, infinite loop on extensionHeader_buffer
  while (extHdrType != NO_MORE_EXT_HDRS) {
    if (extensionHeader_length > 0) {
      memcpy(curPtr, extensionHeader_buffer, extensionHeader_length);
      curPtr += extensionHeader_length;
      LOG_D(GTPU, "Extension Header for DDD added. The length is: %d, extension header type is: %x \n", extensionHeader_length, *((uint8_t *)(buffer + 11)));
      extHdrType = extensionHeader_buffer[extensionHeader_length - 1];
      LOG_D(GTPU, "Next extension header type is: %x \n", *((uint8_t *)(buffer + 11)));
    } else {
      LOG_W(GTPU, "Extension header type not supported, returning... \n");
    }
  }

  if (Msg!= NULL){
    memcpy(curPtr, Msg, msgLen);
    curPtr+=msgLen;
  }

  msgHdr->msgLength = htons(curPtr-(buffer+sizeof(Gtpv1uMsgHeaderT)));
  AssertFatal(curPtr-(buffer+msgLen) < HDR_MAX, "fixed max size of all headers too short");
  // Fix me: add IPv6 support, using flag ipVersion
  struct sockaddr_in to= {0};
  to.sin_family      = AF_INET;
  to.sin_port        = htons(peerPort);
  to.sin_addr.s_addr = peerIp ;
  LOG_D(GTPU,"sending packet size: %ld to %s\n",curPtr-buffer, inet_ntoa(to.sin_addr) );
  int ret;

  if ((ret=sendto(h, (void *)buffer, curPtr-buffer, 0,(struct sockaddr *)&to, sizeof(to) )) != curPtr-buffer ) {
    LOG_E(GTPU, "[SD %d] Failed to send data to " IPV4_ADDR " on port %d, buffer size %lu, ret: %d, errno: %d\n",
          h, IPV4_ADDR_FORMAT(peerIp), peerPort, curPtr-buffer, ret, errno);
    return GTPNOK;
  }

  return  !GTPNOK;
}

void gtpv1uSendDirect(instance_t instance,
                      ue_id_t ue_id,
                      int bearer_id,
                      uint8_t *buf,
                      size_t len,
                      bool seqNumFlag,
                      bool npduNumFlag)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);

  auto ptr2 = ptrUe->second.bearers.find(bearer_id);

  if (ptr2 == ptrUe->second.bearers.end()) {
    LOG_E(GTPU, "[%ld] GTP-U instance: sending a packet to a non existant UE:RAB: %lx/%x\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  LOG_D(GTPU,
        "[%ld] sending a packet to UE:RAB:teid %lx/%x/%x, len %lu, oldseq %d, oldnum %d\n",
        instance,
        ue_id,
        bearer_id,
        ptr2->second.teid_outgoing,
        len,
        ptr2->second.seqNum,
        ptr2->second.npduNum);

  if (seqNumFlag)
    ptr2->second.seqNum++;

  if (npduNumFlag)
    ptr2->second.npduNum++;

  // copy to release the mutex
  gtpv1u_bearer_t tmp = ptr2->second;
  pthread_mutex_unlock(&globGtp.gtp_lock);

  if (tmp.outgoing_qfi != -1) {
    Gtpv1uExtHeaderT ext = {0};
    ext.ExtHeaderLen = 1; // in quad bytes  EXT_HDR_LNTH_OCTET_UNITS
    ext.pdusession_cntr.spare = 0;
    ext.pdusession_cntr.PDU_type = UL_PDU_SESSION_INFORMATION;
    ext.pdusession_cntr.QFI = tmp.outgoing_qfi;
    ext.pdusession_cntr.Reflective_QoS_activation = false;
    ext.pdusession_cntr.Paging_Policy_Indicator = false;
    ext.NextExtHeaderType = NO_MORE_EXT_HDRS;

    gtpv1uCreateAndSendMsg(compatInst(instance),
                           tmp.outgoing_ip_addr,
                           tmp.outgoing_port,
                           GTP_GPDU,
                           tmp.teid_outgoing,
                           buf,
                           len,
                           seqNumFlag,
                           npduNumFlag,
                           tmp.seqNum,
                           tmp.npduNum,
                           PDU_SESSION_CONTAINER,
                           (uint8_t *)&ext,
                           sizeof(ext));
  } else {
    gtpv1uCreateAndSendMsg(compatInst(instance),
                           tmp.outgoing_ip_addr,
                           tmp.outgoing_port,
                           GTP_GPDU,
                           tmp.teid_outgoing,
                           buf,
                           len,
                           seqNumFlag,
                           npduNumFlag,
                           tmp.seqNum,
                           tmp.npduNum,
                           NO_MORE_EXT_HDRS,
                           NULL,
                           0);
  }
}

//////////////////////////
////// edited by zyzy////
/////////////////////////
uint16_t Eth_PPPOE_Discovery = ETH_PPPOE_DISCOVERY;
uint16_t Eth_PPPOE_Session   = ETH_PPPOE_SESSION;

unsigned char *
findTag(PPPoEPacket *packet, uint16_t type, PPPoETag *tag)
{
    uint16_t len = ntohs(packet->length);
    unsigned char *curTag;
    uint16_t tagType, tagLen;

    if (PPPOE_VER(packet->vertype) != 1) {
	LOG_E(GTPU, "Invalid PPPoE version (%d)", PPPOE_VER(packet->vertype));
	return NULL;
    }
    if (PPPOE_TYPE(packet->vertype) != 1) {
	LOG_E(GTPU, "Invalid PPPoE type (%d)", PPPOE_TYPE(packet->vertype));
	return NULL;
    }

    /* Do some sanity checks on packet */
    if (len > ETH_JUMBO_LEN - 6) { /* 6-byte overhead for PPPoE header */
	LOG_E(GTPU, "Invalid PPPoE packet length (%u)", len);
	return NULL;
    }

    /* Step through the tags */
    curTag = packet->payload;
    while(curTag - packet->payload + TAG_HDR_SIZE <= len) {
	/* Alignment is not guaranteed, so do this by hand... */
	tagType = (((uint16_t) curTag[0]) << 8) +
	    (uint16_t) curTag[1];
	tagLen = (((uint16_t) curTag[2]) << 8) +
	    (uint16_t) curTag[3];
	if (tagType == TAG_END_OF_LIST) {
	    return NULL;
	}
	if ((curTag - packet->payload) + tagLen + TAG_HDR_SIZE > len) {
	    LOG_E(GTPU, "Invalid PPPoE tag length (%u)", tagLen);
	    return NULL;
	}
	if (tagType == type) {
	    memcpy(tag, curTag, tagLen + TAG_HDR_SIZE);
	    return curTag;
	}
	curTag = curTag + TAG_HDR_SIZE + tagLen;
    }
    return NULL;
}

int
sendPacket(PPPoEConnection *conn, int sock, PPPoEPacket *pkt, int size)
{
#if defined(HAVE_STRUCT_SOCKADDR_LL)
    if (send(sock, pkt, size, 0) < 0 && (errno != ENOBUFS)) {
	sysErr("send (sendPacket)");
	return -1;
    }
#else
    struct sockaddr sa;

    if (!conn) {
	rp_fatal("relay and server not supported on Linux 2.0 kernels");
    }
    if (strlen(conn->ifName) >= sizeof(sa.sa_data)) {
        rp_fatal("Interface name too long");
    }
    strcpy(sa.sa_data, conn->ifName);
    if (sendto(sock, pkt, size, 0, &sa, sizeof(sa)) < 0) {
	sysErr("sendto (sendPacket)");
	return -1;
    }
#endif
    return 0;
}

int
receivePacket(int sock, PPPoEPacket *pkt, int *size)
{
    if ((*size = recv(sock, pkt, sizeof(PPPoEPacket), 0)) < 0) {
	sysErr("recv (receivePacket)");
	return -1;
    }
    return 0;
}

int
openInterface(char const *ifname, uint16_t type, unsigned char *hwaddr, uint16_t *mtu)
{
    int optval=1;
    int fd;
    struct ifreq ifr;
    int domain, stype;

#ifdef HAVE_STRUCT_SOCKADDR_LL
    struct sockaddr_ll sa;
#else
    struct sockaddr sa;
#endif

    memset(&sa, 0, sizeof(sa));

#ifdef HAVE_STRUCT_SOCKADDR_LL
    domain = PF_PACKET;
    stype = SOCK_RAW;
#else
    domain = PF_INET;
    stype = SOCK_PACKET;
#endif

    if ((fd = socket(domain, stype, htons(type))) < 0) {
	/* Give a more helpful message for the common error case */
	if (errno == EPERM) {
	    rp_fatal("Cannot create raw socket -- pppoe must be run as root.");
	}
	fatalSys("socket");
    }

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
	fatalSys("setsockopt");
    }

    /* Fill in hardware address */
    if (hwaddr) {
	rp_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
	    fatalSys("ioctl(SIOCGIFHWADDR)");
	}
	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
#ifdef ARPHRD_ETHER
	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
	    char buffer[256];
	    sprintf(buffer, "Interface %.16s is not Ethernet", ifname);
	    rp_fatal(buffer);
	}
#endif
	if (NOT_UNICAST(hwaddr)) {
	    char buffer[256];
	    sprintf(buffer,
		    "Interface %.16s has broadcast/multicast MAC address??",
		    ifname);
	    rp_fatal(buffer);
	}
    }

    /* Sanity check on MTU */
    rp_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFMTU, &ifr) < 0) {
	fatalSys("ioctl(SIOCGIFMTU)");
    }
    if (ifr.ifr_mtu < ETH_DATA_LEN) {
	printErr("Interface %.16s has MTU of %d -- should be %d.  You may have serious connection problems.",
		ifname, ifr.ifr_mtu, ETH_DATA_LEN);
    }
    if (mtu) *mtu = ifr.ifr_mtu;

#ifdef HAVE_STRUCT_SOCKADDR_LL
    /* Get interface index */
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(type);

    rp_strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
	fatalSys("ioctl(SIOCFIGINDEX): Could not get interface index");
    }
    sa.sll_ifindex = ifr.ifr_ifindex;

#else
    if (strlen(ifname) >= sizeof(sa.sa_data)) {
        rp_fatal("Interface name too long");
    }
    strcpy(sa.sa_data, ifname);
#endif

    /* We're only interested in packets on specified interface */
    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
	fatalSys("bind");
    }

    return fd;
}

size_t
rp_strlcpy(char *dst, const char *src, size_t size)
{
    const char *orig_src = src;

    if (size == 0) {
	return 0;
    }

    while (--size != 0) {
	if ((*dst++ = *src++) == '\0') {
	    break;
	}
    }

    if (size == 0) {
	*dst = '\0';
    }

    return src - orig_src - 1;
}

void
printErr(char const *fmt, ...)
{
    char *str;
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vasprintf(&str, fmt, ap);
    va_end(ap);

    if (r < 0)
	return;

    fprintf(stderr, "pppoe: %s\n", str);
    LOG_E(GTPU, "%s", str);
    free(str);
}
//relay
PPPoEInterface Interfaces[MAX_INTERFACES];
int NumInterfaces;

/* Relay info */
int NumSessions;
int MaxSessions;
PPPoESession *AllSessions;
PPPoESession *FreeSessions;
PPPoESession *ActiveSessions;

SessionHash *AllHashes;
SessionHash *FreeHashes;
SessionHash *Buckets[HASHTAB_SIZE];

volatile unsigned int Epoch = 0;
volatile unsigned int CleanCounter = 0;

/* How often to clean up stale sessions? */
#define MIN_CLEAN_PERIOD 30  /* Minimum period to run cleaner */
#define TIMEOUT_DIVISOR 20   /* How often to run cleaner per timeout period */
unsigned int CleanPeriod = MIN_CLEAN_PERIOD;

/* How long a session can be idle before it is cleaned up? */
unsigned int IdleTimeout = MIN_CLEAN_PERIOD * TIMEOUT_DIVISOR;

/* Pipe for breaking select() to initiate periodic cleaning */
int CleanPipe[2];

/* Our relay: if_index followed by peer_mac */
#define MY_RELAY_TAG_LEN (sizeof(int) + ETH_ALEN)

/* Hack for daemonizing */
#define CLOSEFD 64

//keepDescriptor

static int
keepDescriptor(int fd)
{
    int i;
    if (fd == CleanPipe[0] || fd == CleanPipe[1]) return 1;
    for (i=0; i<NumInterfaces; i++) {
	if (fd == Interfaces[i].discoverySock ||
	    fd == Interfaces[i].sessionSock) return 1;
    }
    return 0;
}

//add tag
int
addTag(PPPoEPacket *packet, PPPoETag const *tag)
{
    return insertBytes(packet, packet->payload, tag,
		       ntohs(tag->length) + TAG_HDR_SIZE);
}

//insertBytes
int
insertBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    void const *bytes,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    /* Sanity checks */
    if (loc < packet->payload ||
	loc > packet->payload + plen ||
	len + plen > MAX_PPPOE_PAYLOAD) {
	return -1;
    }

    toMove = (packet->payload + plen) - loc;
    memmove(loc+len, loc, toMove);
    memcpy(loc, bytes, len);
    packet->length = htons(plen + len);
    return len;
}

//removeBytes
 int
removeBytes(PPPoEPacket *packet,
	    unsigned char *loc,
	    int len)
{
    int toMove;
    int plen = ntohs(packet->length);
    /* Sanity checks */
    if (len < 0 || len > plen ||
	loc < packet->payload ||
	loc + len > packet->payload + plen) {
	return -1;
    }

    toMove = ((packet->payload + plen) - loc) - len;
    memmove(loc, loc+len, toMove);
    packet->length = htons(plen - len);
    return len;
}

/*main*/

int
pppoerelaygtpu(int argc, char *argv[])
{
    //int opt;
    int nsess = DEFAULT_SESSIONS;
    struct sigaction sa;
    int beDaemon = 1;

    if (getuid() != geteuid() ||
	getgid() != getegid()) {
	fprintf(stderr, "SECURITY WARNING: pppoe-relay will NOT run suid or sgid.  Fix your installation.\n");
	exit(EXIT_FAILURE);
    }

    openlog("pppoe-relay", LOG_PID, LOG_DAEMON);

    /* Check that at least two interfaces were defined */
  
    if (NumInterfaces < 2) {
	fprintf(stderr, "%s: Must define at least two interfaces\n",
		argv[0]);
	exit(EXIT_FAILURE);
    }

    /* Make a pipe for the cleaner*/ 
    if (pipe(CleanPipe) < 0) {
	fatalSys("pipe");
    }

    /* Set up alarm handler*/ 
    sa.sa_handler = alarmHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
	fatalSys("sigaction");
    }

    /* Allocate memory for sessions, etc.*/ 
    initRelay(nsess);

    /* Daemonize -- UNIX Network Programming, Vol. 1, Stevens */
    if (beDaemon) {
	int i;
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
  
	    /* parent */
	 
      exit(EXIT_SUCCESS);
	}
	setsid();
	signal(SIGHUP, SIG_IGN);
	i = fork();
	if (i < 0) {
	    fatalSys("fork");
	} else if (i != 0) {
	    exit(EXIT_SUCCESS);
	}

	if (chdir("/") < 0) {
	    fatalSys("chdir");
	}
	closelog();
	for (i=0; i<CLOSEFD; i++) {
	    if (!keepDescriptor(i)) {
		close(i);
	    }
	}
 

	/* We nuked our syslog descriptor...*/ 
	openlog("pppoe-relay", LOG_PID, LOG_DAEMON);
    }

    /* Kick off SIGALRM if there is an idle timeout */
    if (IdleTimeout) alarm(1);

    /* Enter the relay loop */
    relayLoop();

    /* Shouldn't ever get here... */
    return EXIT_FAILURE;
}


//addInterface
void
addInterface(char const *ifname,
	     int clientOK,
	     int acOK)
{
    PPPoEInterface *i;
    int j;
    for (j=0; j<NumInterfaces; j++) {
	if (!strncmp(Interfaces[j].name, ifname, IFNAMSIZ)) {
	    fprintf(stderr, "Interface %s specified more than once.\n", ifname);
	    exit(EXIT_FAILURE);
	}
    }

    if (NumInterfaces >= MAX_INTERFACES) {
	fprintf(stderr, "Too many interfaces (%d max)\n",
		MAX_INTERFACES);
	exit(EXIT_FAILURE);
    }
    i = &Interfaces[NumInterfaces++];
    strncpy(i->name, ifname, IFNAMSIZ);
    i->name[IFNAMSIZ] = 0;

    i->discoverySock = openInterface(ifname, Eth_PPPOE_Discovery, i->mac, NULL);
    i->sessionSock   = openInterface(ifname, Eth_PPPOE_Session,   NULL, NULL);
    i->clientOK = clientOK;
    i->acOK = acOK;
}

//initRelay
void
initRelay(int nsess)
{
    int i;
    NumSessions = 0;
    MaxSessions = nsess;

    AllSessions = (PPPoESession*) calloc(MaxSessions, sizeof(PPPoESession));
    if (!AllSessions) {
	rp_fatal("Unable to allocate memory for PPPoE session table");
    }
    AllHashes = (SessionHash*) calloc(MaxSessions * 2, sizeof(SessionHash));
    if (!AllHashes) {
	rp_fatal("Unable to allocate memory for PPPoE hash table");
    }

    /* Initialize sessions in a linked list */
    AllSessions[0].prev = NULL;
    if (MaxSessions > 1) {
	AllSessions[0].next = &AllSessions[1];
    } else {
	AllSessions[0].next = NULL;
    }
    for (i=1; i<MaxSessions-1; i++) {
	AllSessions[i].prev = &AllSessions[i-1];
	AllSessions[i].next = &AllSessions[i+1];
    }
    if (MaxSessions > 1) {
	AllSessions[MaxSessions-1].prev = &AllSessions[MaxSessions-2];
	AllSessions[MaxSessions-1].next = NULL;
    }

    FreeSessions = AllSessions;
    ActiveSessions = NULL;

    /* Initialize session numbers which we hand out */
    for (i=0; i<MaxSessions; i++) {
	AllSessions[i].sesNum = htons((uint16_t) i+1);
    }

    /* Initialize hashes in a linked list */
    AllHashes[0].prev = NULL;
    AllHashes[0].next = &AllHashes[1];
    for (i=1; i<2*MaxSessions-1; i++) {
	AllHashes[i].prev = &AllHashes[i-1];
	AllHashes[i].next = &AllHashes[i+1];
    }
    AllHashes[2*MaxSessions-1].prev = &AllHashes[2*MaxSessions-2];
    AllHashes[2*MaxSessions-1].next = NULL;

    FreeHashes = AllHashes;
}

//CreateSession
PPPoESession *
createSession(PPPoEInterface const *ac,
	      PPPoEInterface const *cli,
	      unsigned char const *acMac,
	      unsigned char const *cliMac,
	      uint16_t acSes)
{
    PPPoESession *sess;
    SessionHash *acHash, *cliHash;

    if (NumSessions >= MaxSessions) {
	//printErr("Maximum number of sessions reached -- cannot create new session");
	return NULL;
    }

    /* Grab a free session */
    sess = FreeSessions;
    FreeSessions = sess->next;
    NumSessions++;

    /* Link it to the active list */
    sess->next = ActiveSessions;
    if (sess->next) {
	sess->next->prev = sess;
    }
    ActiveSessions = sess;
    sess->prev = NULL;

    sess->epoch = Epoch;

    /* Get two hash entries */
    acHash = FreeHashes;
    cliHash = acHash->next;
    FreeHashes = cliHash->next;

    acHash->peer = cliHash;
    cliHash->peer = acHash;

    sess->acHash = acHash;
    sess->clientHash = cliHash;

    acHash->interface = ac;
    cliHash->interface = cli;

    memcpy(acHash->peerMac, acMac, ETH_ALEN);
    acHash->sesNum = acSes;
    acHash->ses = sess;

    memcpy(cliHash->peerMac, cliMac, ETH_ALEN);
    cliHash->sesNum = sess->sesNum;
    cliHash->ses = sess;

    addHash(acHash);
    addHash(cliHash);

    /* Log */
    syslog(LOG_INFO,
	   "Opened session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d)",
	   acHash->peerMac[0], acHash->peerMac[1],
	   acHash->peerMac[2], acHash->peerMac[3],
	   acHash->peerMac[4], acHash->peerMac[5],
	   acHash->interface->name,
	   ntohs(acHash->sesNum),
	   cliHash->peerMac[0], cliHash->peerMac[1],
	   cliHash->peerMac[2], cliHash->peerMac[3],
	   cliHash->peerMac[4], cliHash->peerMac[5],
	   cliHash->interface->name,
	   ntohs(cliHash->sesNum));

    return sess;
}

//freeSession
void
freeSession(PPPoESession *ses, char const *msg)
{
    syslog(LOG_INFO,
	   "Closed session: server=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d), client=%02x:%02x:%02x:%02x:%02x:%02x(%s:%d): %s",
	   ses->acHash->peerMac[0], ses->acHash->peerMac[1],
	   ses->acHash->peerMac[2], ses->acHash->peerMac[3],
	   ses->acHash->peerMac[4], ses->acHash->peerMac[5],
	   ses->acHash->interface->name,
	   ntohs(ses->acHash->sesNum),
	   ses->clientHash->peerMac[0], ses->clientHash->peerMac[1],
	   ses->clientHash->peerMac[2], ses->clientHash->peerMac[3],
	   ses->clientHash->peerMac[4], ses->clientHash->peerMac[5],
	   ses->clientHash->interface->name,
	   ntohs(ses->clientHash->sesNum), msg);

    /* Unlink from active sessions */
    if (ses->prev) {
	ses->prev->next = ses->next;
    } else {
	ActiveSessions = ses->next;
    }
    if (ses->next) {
	ses->next->prev = ses->prev;
    }

    /* Link onto free list -- this is a singly-linked list, so
       we do not care about prev */
    ses->next = FreeSessions;
    FreeSessions = ses;

    unhash(ses->acHash);
    unhash(ses->clientHash);
    NumSessions--;
}

//unhash
void
unhash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    if (sh->prev) {
	sh->prev->next = sh->next;
    } else {
	Buckets[b] = sh->next;
    }

    if (sh->next) {
	sh->next->prev = sh->prev;
    }

    /* Add to free list (singly-linked) */
    sh->next = FreeHashes;
    FreeHashes = sh;
}

//addHash
void
addHash(SessionHash *sh)
{
    unsigned int b = hash(sh->peerMac, sh->sesNum) % HASHTAB_SIZE;
    sh->next = Buckets[b];
    sh->prev = NULL;
    if (sh->next) {
	sh->next->prev = sh;
    }
    Buckets[b] = sh;
}

//hash
unsigned int
hash(unsigned char const *mac, uint16_t sesNum)
{
    unsigned int ans1 =
	((unsigned int) mac[0]) |
	(((unsigned int) mac[1]) << 8) |
	(((unsigned int) mac[2]) << 16) |
	(((unsigned int) mac[3]) << 24);
    unsigned int ans2 =
	((unsigned int) sesNum) |
	(((unsigned int) mac[4]) << 16) |
	(((unsigned int) mac[5]) << 24);
    return ans1 ^ ans2;
}

//findSession
SessionHash *
findSession(unsigned char const *mac, uint16_t sesNum)
{
    unsigned int b = hash(mac, sesNum) % HASHTAB_SIZE;
    SessionHash *sh = Buckets[b];
    while(sh) {
	if (!memcmp(mac, sh->peerMac, ETH_ALEN) && sesNum == sh->sesNum) {
	    return sh;
	}
	sh = sh->next;
    }
    return NULL;
}

//FatalSys
void
fatalSys(char const *str)
{
    //printErr("%.256s: %.256s", str, strerror(errno));
    exit(EXIT_FAILURE);
}

//sysErr

void
sysErr(char const *str)
{
    printErr("%.256s: %.256s", str, strerror(errno));
}

//rpFatal
void
rp_fatal(char const *str)
{
    //printErr("%s", str);
    exit(EXIT_FAILURE);
}

//RelayLoop
void
relayLoop()
{
    fd_set readable, readableCopy;
    int maxFD;
    int i, r;
    int sock;

    /* Build the select set */
    FD_ZERO(&readable);
    maxFD = 0;
    for (i=0; i<NumInterfaces; i++) {
	sock = Interfaces[i].discoverySock;
	if (sock > maxFD) maxFD = sock;
	FD_SET(sock, &readable);
	sock = Interfaces[i].sessionSock;
	if (sock > maxFD) maxFD = sock;
	FD_SET(sock, &readable);
	if (CleanPipe[0] > maxFD) maxFD = CleanPipe[0];
	FD_SET(CleanPipe[0], &readable);
    }
    maxFD++;
    for(;;) {
	readableCopy = readable;
	for(;;) {
	    r = select(maxFD, &readableCopy, NULL, NULL, NULL);
	    if (r >= 0 || errno != EINTR) break;
	}
	if (r < 0) {
	    //sysErr("select (relayLoop)");
	    continue;
	}

	/* Handle session packets first */
	for (i=0; i<NumInterfaces; i++) {
	    if (FD_ISSET(Interfaces[i].sessionSock, &readableCopy)) {
		relayGotSessionPacket(&Interfaces[i]);
	    }
	}

	/* Now handle discovery packets */
	for (i=0; i<NumInterfaces; i++) {
	    if (FD_ISSET(Interfaces[i].discoverySock, &readableCopy)) {
		relayGotDiscoveryPacket(&Interfaces[i]);
	    }
	}

	/* Handle the session-cleaning process */
	if (FD_ISSET(CleanPipe[0], &readableCopy)) {
	    char dummy;
	    CleanCounter = 0;
#pragma GCC diagnostic ignored "-Wunused-result"      
	    read(CleanPipe[0], &dummy, 1);
#pragma GCC diagnostic warning "-Wunused-result"      
	    if (IdleTimeout) cleanSessions();
	}
    }
}

//relayGotDiscoveryPacket
void
relayGotDiscoveryPacket(PPPoEInterface const *iface)
{
    PPPoEPacket packet;
    int size;

    if (receivePacket(iface->discoverySock, &packet, &size) < 0) {
	return;
    }
    /* Ignore unknown code/version */
    if (PPPOE_VER(packet.vertype) != 1 || PPPOE_TYPE(packet.vertype) != 1) {
	return;
    }

    /* Validate length */
    if ((unsigned int)(ntohs(packet.length) + HDR_SIZE) > (unsigned int)size) {
	LOG_E(GTPU, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    /* Drop Ethernet frame padding */
    if ((unsigned int)size > (unsigned int)(ntohs(packet.length) + HDR_SIZE)) {
	size = ntohs(packet.length) + HDR_SIZE;
    }

    switch(packet.code) {
    case CODE_PADT:
	relayHandlePADT(iface, &packet, size);
	break;
    case CODE_PADI:
	relayHandlePADI(iface, &packet, size);
	break;
    case CODE_PADO:
	relayHandlePADO(iface, &packet, size);
	break;
    case CODE_PADR:
	relayHandlePADR(iface, &packet, size);
	break;
    case CODE_PADS:
	relayHandlePADS(iface, &packet, size);
	break;
    default:
	LOG_E(GTPU, "Discovery packet on %s with unknown code %d",
	       iface->name, (int) packet.code);
    }
}

//relayGotSessionPacket
void
relayGotSessionPacket(PPPoEInterface const *iface)
{
    PPPoEPacket packet;
    int size;
    SessionHash *sh;
    PPPoESession *ses;

    if (receivePacket(iface->sessionSock, &packet, &size) < 0) {
	return;
    }

    /* Ignore unknown code/version */
    if (PPPOE_VER(packet.vertype) != 1 || PPPOE_TYPE(packet.vertype) != 1) {
	return;
    }

    /* Must be a session packet */
    if (packet.code != CODE_SESS) {
	LOG_E(GTPU, "Session packet with code %d", (int) packet.code);
	return;
    }

    /* Ignore session packets whose destination address isn't ours */
    if (memcmp(packet.ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Validate length */
    if ((unsigned int)(ntohs(packet.length) + HDR_SIZE) > (unsigned int)size) {
	LOG_E(GTPU, "Bogus PPPoE length field (%u)",
	       (unsigned int) ntohs(packet.length));
	return;
    }

    /* Drop Ethernet frame padding */
    if ((unsigned int)size > (unsigned int)(ntohs(packet.length) + HDR_SIZE)) {
	size = ntohs(packet.length) + HDR_SIZE;
    }

    /* We're in business!  Find the hash */
    sh = findSession(packet.ethHdr.h_source, packet.session);
    if (!sh) {
	/* Don't log this.  Someone could be running the client and the
	   relay on the same box. */
	return;
    }

    /* Relay it */
    ses = sh->ses;
    ses->epoch = Epoch;
    sh = sh->peer;
    packet.session = sh->sesNum;
    memcpy(packet.ethHdr.h_source, sh->interface->mac, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, sh->peerMac, ETH_ALEN);
#if 0
    fprintf(stderr, "Relaying %02x:%02x:%02x:%02x:%02x:%02x(%s:%d) to %02x:%02x:%02x:%02x:%02x:%02x(%s:%d)\n",
	    sh->peer->peerMac[0], sh->peer->peerMac[1], sh->peer->peerMac[2],
	    sh->peer->peerMac[3], sh->peer->peerMac[4], sh->peer->peerMac[5],
	    sh->peer->interface->name, ntohs(sh->peer->sesNum),
	    sh->peerMac[0], sh->peerMac[1], sh->peerMac[2],
	    sh->peerMac[3], sh->peerMac[4], sh->peerMac[5],
	    sh->interface->name, ntohs(sh->sesNum));
#endif
    sendPacket(NULL, sh->interface->sessionSock, &packet, size);
}

void
relayHandlePADT(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    SessionHash *sh;
    PPPoESession *ses;

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    sh = findSession(packet->ethHdr.h_source, packet->session);
    if (!sh) {
	return;
    }
    /* Relay the PADT to the peer */
    sh = sh->peer;
    ses = sh->ses;
    packet->session = sh->sesNum;
    memcpy(packet->ethHdr.h_source, sh->interface->mac, ETH_ALEN);
    memcpy(packet->ethHdr.h_dest, sh->peerMac, ETH_ALEN);
    sendPacket(NULL, sh->interface->sessionSock, packet, size);

    /* Destroy the session */
    freeSession(ses, "Received PADT");
}


void
relayHandlePADI(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int i, r;

    int ifIndex;

    /* Can a client legally be behind this interface? */
    if (!iface->clientOK) {
	LOG_E(GTPU,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	LOG_E(GTPU,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be broadcast */
    if (NOT_BROADCAST(packet->ethHdr.h_dest)) {
	LOG_E(GTPU,
	       "PADI packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not to a broadcast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Get array index of interface */
    ifIndex = iface - Interfaces;

    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	tag.type = htons(TAG_RELAY_SESSION_ID);
	tag.length = htons(MY_RELAY_TAG_LEN);
	memcpy(tag.payload, &ifIndex, sizeof(ifIndex));
	memcpy(tag.payload+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);
	/* Add a relay tag if there's room */
	r = addTag(packet, &tag);
	if (r < 0) return;
	size += r;
    } else {
	/* We do not reuse relay-id tags.  Drop the frame.  The RFC says the
	   relay agent SHOULD return a Generic-Error tag, but this does not
	   make sense for PADI packets. */
	return;
    }

    /* Broadcast the PADI on all AC-capable interfaces except the interface
       on which it came */
    for (i=0; i < NumInterfaces; i++) {
	if (iface == &Interfaces[i]) continue;
	if (!Interfaces[i].acOK) continue;
	memcpy(packet->ethHdr.h_source, Interfaces[i].mac, ETH_ALEN);
	sendPacket(NULL, Interfaces[i].discoverySock, packet, size);
    }

}

void
relayHandlePADO(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int acIndex;

    /* Can a server legally be behind this interface? */
    if (!iface->acOK) {
	LOG_E(GTPU,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    acIndex = iface - Interfaces;

    /* Source address can't be broadcast */
    if (BROADCAST(packet->ethHdr.h_source)) {
	LOG_E(GTPU,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s from a broadcast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	LOG_E(GTPU,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	LOG_E(GTPU,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].clientOK ||
	iface == &Interfaces[ifIndex]) {
	LOG_E(GTPU,
	       "PADO packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Replace Relay-ID tag with opposite-direction tag */
    memcpy(loc+TAG_HDR_SIZE, &acIndex, sizeof(acIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    /* Send the PADO to the proper client */
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

void
relayHandlePADR(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;
    int cliIndex;

    /* Can a client legally be behind this interface? */
    if (!iface->clientOK) {
	LOG_E(GTPU,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    cliIndex = iface - Interfaces;

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	LOG_E(GTPU,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	LOG_E(GTPU,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	LOG_E(GTPU,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].acOK ||
	iface == &Interfaces[ifIndex]) {
	LOG_E(GTPU,
	       "PADR packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Replace Relay-ID tag with opposite-direction tag */
    memcpy(loc+TAG_HDR_SIZE, &cliIndex, sizeof(cliIndex));
    memcpy(loc+TAG_HDR_SIZE+sizeof(ifIndex), packet->ethHdr.h_source, ETH_ALEN);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    /* Send the PADR to the proper access concentrator */
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

void
relayHandlePADS(PPPoEInterface const *iface,
		PPPoEPacket *packet,
		int size)
{
    PPPoETag tag;
    unsigned char *loc;
    int ifIndex;

    PPPoESession *ses = NULL;
    SessionHash *sh;

    /* Can a server legally be behind this interface? */
    if (!iface->acOK) {
	LOG_E(GTPU,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not permitted",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Source address must be unicast */
    if (NOT_UNICAST(packet->ethHdr.h_source)) {
	LOG_E(GTPU,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s not from a unicast address",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Destination address must be interface's MAC address */
    if (memcmp(packet->ethHdr.h_dest, iface->mac, ETH_ALEN)) {
	return;
    }

    /* Find relay tag */
    loc = findTag(packet, TAG_RELAY_SESSION_ID, &tag);
    if (!loc) {
	LOG_E(GTPU,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If it's the wrong length, ignore it */
    if (ntohs(tag.length) != MY_RELAY_TAG_LEN) {
	LOG_E(GTPU,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s does not have correct length Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* Extract interface index */
    memcpy(&ifIndex, tag.payload, sizeof(ifIndex));

    if (ifIndex < 0 || ifIndex >= NumInterfaces ||
	!Interfaces[ifIndex].clientOK ||
	iface == &Interfaces[ifIndex]) {
	LOG_E(GTPU,
	       "PADS packet from %02x:%02x:%02x:%02x:%02x:%02x on interface %s has invalid interface in Relay-Session-Id tag",
	       packet->ethHdr.h_source[0],
	       packet->ethHdr.h_source[1],
	       packet->ethHdr.h_source[2],
	       packet->ethHdr.h_source[3],
	       packet->ethHdr.h_source[4],
	       packet->ethHdr.h_source[5],
	       iface->name);
	return;
    }

    /* If session ID is zero, it's the AC responding with an error.
       Just relay it; do not create a session */
    if (packet->session != htons(0)) {
	/* Check for existing session */
	sh = findSession(packet->ethHdr.h_source, packet->session);
	if (sh) ses = sh->ses;

	/* If already an existing session, assume it's a duplicate PADS.  Send
	   the frame, but do not create a new session.  Is this the right
	   thing to do?  Arguably, should send an error to the client and
	   a PADT to the server, because this could happen due to a
	   server crash and reboot. */

	if (!ses) {
	    /* Create a new session */
	    ses = createSession(iface, &Interfaces[ifIndex],
				packet->ethHdr.h_source,
				loc + TAG_HDR_SIZE + sizeof(ifIndex), packet->session);
	    if (!ses) {
		/* Can't allocate session -- send error PADS to client and
		   PADT to server */
		PPPoETag hostUniq, *hu;
		if (findTag(packet, TAG_HOST_UNIQ, &hostUniq)) {
		    hu = &hostUniq;
		} else {
		    hu = NULL;
		}
		relaySendError(CODE_PADS, htons(0), &Interfaces[ifIndex],
			       loc + TAG_HDR_SIZE + sizeof(ifIndex),
			       hu, "RP-PPPoE: Relay: Unable to allocate session");
		relaySendError(CODE_PADT, packet->session, iface,
			       packet->ethHdr.h_source, NULL,
			       "RP-PPPoE: Relay: Unable to allocate session");
		return;
	    }
	}
	/* Replace session number */
	packet->session = ses->sesNum;
    }

    /* Remove relay-ID tag */
    removeBytes(packet, loc, MY_RELAY_TAG_LEN + TAG_HDR_SIZE);
    size -= (MY_RELAY_TAG_LEN + TAG_HDR_SIZE);

    /* Set destination address to MAC address in relay ID */
    memcpy(packet->ethHdr.h_dest, tag.payload + sizeof(ifIndex), ETH_ALEN);

    /* Set source address to MAC address of interface */
    memcpy(packet->ethHdr.h_source, Interfaces[ifIndex].mac, ETH_ALEN);

    /* Send the PADS to the proper client */
    sendPacket(NULL, Interfaces[ifIndex].discoverySock, packet, size);
}

void
relaySendError(unsigned char code,
	       uint16_t session,
	       PPPoEInterface const *iface,
	       unsigned char const *mac,
	       PPPoETag const *hostUniq,
	       char const *errMsg)
{
    PPPoEPacket packet;
    PPPoETag errTag;
    int size;

    memcpy(packet.ethHdr.h_source, iface->mac, ETH_ALEN);
    memcpy(packet.ethHdr.h_dest, mac, ETH_ALEN);
    packet.ethHdr.h_proto = htons(Eth_PPPOE_Discovery);
    packet.vertype = PPPOE_VER_TYPE(1, 1);
    packet.code = code;
    packet.session = session;
    packet.length = htons(0);
    if (hostUniq) {
	if (addTag(&packet, hostUniq) < 0) return;
    }
    errTag.type = htons(TAG_GENERIC_ERROR);
    errTag.length = htons(strlen(errMsg));
    strcpy((char *) errTag.payload, errMsg);
    if (addTag(&packet, &errTag) < 0) return;
    size = ntohs(packet.length) + HDR_SIZE;
    if (code == CODE_PADT) {
	sendPacket(NULL, iface->discoverySock, &packet, size);
    } else {
	sendPacket(NULL, iface->sessionSock, &packet, size);
    }
}

void
alarmHandler(int sig)
{
    alarm(1);
    Epoch++;
    CleanCounter++;
    if (CleanCounter == CleanPeriod) {
#pragma GCC diagnostic ignored "-Wunused-result"      
	write(CleanPipe[1], "", 1);
#pragma GCC diagnostic warning "-Wunused-result"      
    }
}

void cleanSessions(void)
{
    PPPoESession *cur, *next;
    cur = ActiveSessions;
    while(cur) {
	next = cur->next;
	if (Epoch - cur->epoch > IdleTimeout) {
	    /* Send PADT to each peer */
	    relaySendError(CODE_PADT, cur->acHash->sesNum,
			   cur->acHash->interface,
			   cur->acHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    relaySendError(CODE_PADT, cur->clientHash->sesNum,
			   cur->clientHash->interface,
			   cur->clientHash->peerMac, NULL,
			   "RP-PPPoE: Relay: Session exceeded idle timeout");
	    freeSession(cur, "Idle Timeout");
	}
	cur = next;
    }
}


static void fillDlDeliveryStatusReport(extensionHeader_t *extensionHeader, uint32_t RLC_buffer_availability, uint32_t NR_PDCP_PDU_SN){

  extensionHeader->buffer[0] = (1+sizeof(DlDataDeliveryStatus_flagsT)+(NR_PDCP_PDU_SN>0?3:0)+(NR_PDCP_PDU_SN>0?1:0)+1)/4;
  DlDataDeliveryStatus_flagsT DlDataDeliveryStatus;
  DlDataDeliveryStatus.deliveredPdcpSn = 0;
  DlDataDeliveryStatus.transmittedPdcpSn= NR_PDCP_PDU_SN>0?1:0;
  DlDataDeliveryStatus.pduType = 1;
  DlDataDeliveryStatus.drbBufferSize = htonl(RLC_buffer_availability);
  memcpy(extensionHeader->buffer+1, &DlDataDeliveryStatus, sizeof(DlDataDeliveryStatus_flagsT));
  uint8_t offset = sizeof(DlDataDeliveryStatus_flagsT)+1;

  if(NR_PDCP_PDU_SN>0){
    extensionHeader->buffer[offset] =   (NR_PDCP_PDU_SN >> 16) & 0xff;
    extensionHeader->buffer[offset+1] = (NR_PDCP_PDU_SN >> 8) & 0xff;
    extensionHeader->buffer[offset+2] = NR_PDCP_PDU_SN & 0xff;
    LOG_D(GTPU, "Octets reporting NR_PDCP_PDU_SN, extensionHeader->buffer[offset]: %u, extensionHeader->buffer[offset+1]:%u, extensionHeader->buffer[offset+2]:%u \n", extensionHeader->buffer[offset], extensionHeader->buffer[offset+1],extensionHeader->buffer[offset+2]);
    extensionHeader->buffer[offset+3] = 0x00; //Padding octet
    offset = offset+3;
  }
  extensionHeader->buffer[offset] = 0x00; //No more extension headers
  /*Total size of DDD_status PDU = size of mandatory part +
   * 3 octets for highest transmitted/delivered PDCP SN +
   * 1 octet for padding + 1 octet for next extension header type,
   * according to TS 38.425: Fig. 5.5.2.2-1 and section 5.5.3.24*/
  extensionHeader->length  = 1+sizeof(DlDataDeliveryStatus_flagsT)+
                              (NR_PDCP_PDU_SN>0?3:0)+
                              (NR_PDCP_PDU_SN>0?1:0)+1;
}

static void gtpv1uSendDlDeliveryStatus(instance_t instance, gtpv1u_DU_buffer_report_req_t *req){
  ue_id_t ue_id=req->ue_id;
  int  bearer_id=req->pdusession_id;
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);

  auto ptr2=ptrUe->second.bearers.find(bearer_id);

  if ( ptr2 == ptrUe->second.bearers.end() ) {
    LOG_D(GTPU,"GTP-U instance: %ld sending a packet to a non existant UE ID:RAB: %lu/%x\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  extensionHeader_t *extensionHeader;
  extensionHeader = (extensionHeader_t *) calloc(1, sizeof(extensionHeader_t));
  fillDlDeliveryStatusReport(extensionHeader, req->buffer_availability,0);

  LOG_I(GTPU,"[%ld] GTP-U sending DL Data Delivery status to UE ID:RAB:teid %lu/%x/%x, oldseq %d, oldnum %d\n",
        instance, ue_id, bearer_id,ptr2->second.teid_outgoing, ptr2->second.seqNum,ptr2->second.npduNum );
  // copy to release the mutex
  gtpv1u_bearer_t tmp=ptr2->second;
  pthread_mutex_unlock(&globGtp.gtp_lock);
  gtpv1uCreateAndSendMsg(
      compatInst(instance), tmp.outgoing_ip_addr, tmp.outgoing_port, GTP_GPDU, tmp.teid_outgoing, NULL, 0, false, false, 0, 0, NR_RAN_CONTAINER, extensionHeader->buffer, extensionHeader->length);
}

static void gtpv1uEndTunnel(instance_t instance, gtpv1u_enb_end_marker_req_t *req)
{
  ue_id_t ue_id=req->rnti;
  int  bearer_id=req->rab_id;
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);

  auto ptr2=ptrUe->second.bearers.find(bearer_id);

  if ( ptr2 == ptrUe->second.bearers.end() ) {
    LOG_E(GTPU,"[%ld] GTP-U sending a packet to a non existant UE:RAB: %lx/%x\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  LOG_D(GTPU,"[%ld] sending a end packet packet to UE:RAB:teid %lx/%x/%x\n",
        instance, ue_id, bearer_id,ptr2->second.teid_outgoing);
  gtpv1u_bearer_t tmp=ptr2->second;
  pthread_mutex_unlock(&globGtp.gtp_lock);
  Gtpv1uMsgHeaderT  msgHdr;
  // N should be 0 for us (it was used only in 2G and 3G)
  msgHdr.PN=0;
  msgHdr.S=0;
  msgHdr.E=0;
  msgHdr.spare=0;
  //PT=0 is for GTP' TS 32.295 (charging)
  msgHdr.PT=1;
  msgHdr.version=1;
  msgHdr.msgType=GTP_END_MARKER;
  msgHdr.msgLength=htons(0);
  msgHdr.teid=htonl(tmp.teid_outgoing);
  // Fix me: add IPv6 support, using flag ipVersion
  static struct sockaddr_in to= {0};
  to.sin_family      = AF_INET;
  to.sin_port        = htons(tmp.outgoing_port);
  to.sin_addr.s_addr = tmp.outgoing_ip_addr;
  char ip4[INET_ADDRSTRLEN];
  //char ip6[INET6_ADDRSTRLEN];
  LOG_D(GTPU,"[%ld] sending end packet to %s\n", instance, inet_ntoa(to.sin_addr) );

  if (sendto(compatInst(instance), (void *)&msgHdr, sizeof(msgHdr), 0,(struct sockaddr *)&to, sizeof(to) ) !=  sizeof(msgHdr)) {
    LOG_E(GTPU,
          "[%ld] Failed to send data to %s on port %d, buffer size %lu\n",
          compatInst(instance), inet_ntop(AF_INET, &tmp.outgoing_ip_addr, ip4, INET_ADDRSTRLEN), tmp.outgoing_port, sizeof(msgHdr));
  }
}

static  int udpServerSocket(openAddr_s addr) {
  LOG_I(GTPU, "Initializing UDP for local address %s with port %s\n", addr.originHost, addr.originService);
  int status;
  struct addrinfo hints= {0}, *servinfo, *p;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(addr.originHost, addr.originService, &hints, &servinfo)) != 0) {
    LOG_E(GTPU,"getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }

  int sockfd=-1;

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1) {
      LOG_W(GTPU,"socket: %s\n", strerror(errno));
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      LOG_W(GTPU,"bind: %s\n", strerror(errno));
      continue;
    } else {
      // We create the gtp instance on the socket
      globGtp.instances[sockfd].addr=addr;

      if (p->ai_family == AF_INET) {
        struct sockaddr_in *ipv4=(struct sockaddr_in *)p->ai_addr;
        memcpy(globGtp.instances[sockfd].foundAddr,
               &ipv4->sin_addr.s_addr, sizeof(ipv4->sin_addr.s_addr));
        globGtp.instances[sockfd].foundAddrLen=sizeof(ipv4->sin_addr.s_addr);
        globGtp.instances[sockfd].ipVersion=4;
        break;
      } else if (p->ai_family == AF_INET6) {
        LOG_W(GTPU,"Local address is IP v6\n");
        struct sockaddr_in6 *ipv6=(struct sockaddr_in6 *)p->ai_addr;
        memcpy(globGtp.instances[sockfd].foundAddr,
               &ipv6->sin6_addr.s6_addr, sizeof(ipv6->sin6_addr.s6_addr));
        globGtp.instances[sockfd].foundAddrLen=sizeof(ipv6->sin6_addr.s6_addr);
        globGtp.instances[sockfd].ipVersion=6;
      } else
        AssertFatal(false,"Local address is not IPv4 or IPv6");
    }

    break; // if we get here, we must have connected successfully
  }

  if (p == NULL) {
    // looped off the end of the list with no successful bind
    LOG_E(GTPU,"failed to bind socket: %s %s \n", addr.originHost, addr.originService);
    return -1;
  }

  freeaddrinfo(servinfo); // all done with this structure

  int sendbuff = 1000*1000*10;
  AssertFatal(0==setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)),"");
  LOG_D(GTPU,"[%d] Created listener for paquets to: %s:%s, send buffer size: %d\n", sockfd, addr.originHost, addr.originService,sendbuff);
  return sockfd;
}

instance_t gtpv1Init(openAddr_t context) {
  pthread_mutex_lock(&globGtp.gtp_lock);
  int id=udpServerSocket(context);

  if (id>=0) {
    itti_subscribe_event_fd(TASK_GTPV1_U, id);
  } else
    LOG_E(GTPU,"can't create GTP-U instance\n");

  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "Created gtpu instance id: %d\n", id);
  return id;
}

void GtpuUpdateTunnelOutgoingAddressAndTeid(instance_t instance, ue_id_t ue_id, ebi_t bearer_id, in_addr_t newOutgoingAddr, teid_t newOutgoingTeid) {
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetVoid(compatInst(instance));
  getUeRetVoid(inst, ue_id);//cu

  auto ptr2=ptrUe->second.bearers.find(bearer_id);

  if ( ptr2 == ptrUe->second.bearers.end() ) {
    LOG_E(GTPU,"[%ld] Update tunnel for a existing ue id %lu, but wrong bearer_id %u\n", instance, ue_id, bearer_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return;
  }

  ptr2->second.outgoing_ip_addr = newOutgoingAddr;
  ptr2->second.teid_outgoing = newOutgoingTeid;
  LOG_I(GTPU, "[%ld] Tunnel Outgoing TEID updated to %x and address to %x\n", instance, ptr2->second.teid_outgoing, ptr2->second.outgoing_ip_addr); //CU
  pthread_mutex_unlock(&globGtp.gtp_lock);
  return;
}

// create gtpu tunnel for 5g
// edited by zyzy
teid_t newGtpuCreateTunnel(instance_t instance,
                           ue_id_t ue_id,
                           int incoming_bearer_id,
                           int outgoing_bearer_id,
                           teid_t outgoing_teid,
                           int outgoing_qfi,
                           transport_layer_addr_t remoteAddr,
                           int port,
                           gtpCallback callBack,
                           gtpCallbackSDAP callBackSDAP) {
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  auto it=inst->ue2te_mapping.find(ue_id);

  if ( it != inst->ue2te_mapping.end() &&  it->second.bearers.find(outgoing_bearer_id) != it->second.bearers.end()) {
    LOG_W(GTPU,"[%ld] Create a config for a already existing GTP tunnel (ue id %lu)\n", instance, ue_id);
    inst->ue2te_mapping.erase(it);
  }

  teid_t incoming_teid=gtpv1uNewTeid();

  while (globGtp.te2ue_mapping.find(incoming_teid) != globGtp.te2ue_mapping.end()) {
    LOG_W(GTPU, "[%ld] generated a random Teid that exists, re-generating (%x)\n", instance, incoming_teid);
    incoming_teid=gtpv1uNewTeid();
  };

  globGtp.te2ue_mapping[incoming_teid].ue_id = ue_id;
  globGtp.te2ue_mapping[incoming_teid].incoming_rb_id = incoming_bearer_id;
  globGtp.te2ue_mapping[incoming_teid].outgoing_teid = outgoing_teid;
  globGtp.te2ue_mapping[incoming_teid].callBack = callBack;
  globGtp.te2ue_mapping[incoming_teid].callBackSDAP = callBackSDAP;
  globGtp.te2ue_mapping[incoming_teid].pdusession_id = (uint8_t)outgoing_bearer_id;

  gtpv1u_bearer_t *tmp=&inst->ue2te_mapping[ue_id].bearers[outgoing_bearer_id];

  int addrs_length_in_bytes = remoteAddr.length / 8;

  switch (addrs_length_in_bytes) {
    case 4:
      memcpy(&tmp->outgoing_ip_addr,remoteAddr.buffer,4);
      break;

    case 16:
      memcpy(tmp->outgoing_ip6_addr.s6_addr,remoteAddr.buffer,
             16);
      break;

    case 20:
      memcpy(&tmp->outgoing_ip_addr,remoteAddr.buffer,4);
      memcpy(tmp->outgoing_ip6_addr.s6_addr,
             remoteAddr.buffer+4,
             16);

    default:
      AssertFatal(false, "SGW Address size impossible");
  }

  tmp->teid_incoming = incoming_teid;
  tmp->outgoing_port=port;
  tmp->teid_outgoing= outgoing_teid;
  tmp->outgoing_qfi=outgoing_qfi;
  pthread_mutex_unlock(&globGtp.gtp_lock);
  char ip4[INET_ADDRSTRLEN];
  char ip6[INET6_ADDRSTRLEN];
  LOG_I(GTPU,
        "[%ld] Created tunnel for UE ID %lu, teid for incoming: %x, teid for outgoing %x to remote IPv4: %s, IPv6 %s\n",
        instance,
        ue_id,
        tmp->teid_incoming,
        tmp->teid_outgoing,
        inet_ntop(AF_INET, (void *)&tmp->outgoing_ip_addr, ip4, INET_ADDRSTRLEN),
        inet_ntop(AF_INET6, (void *)&tmp->outgoing_ip6_addr.s6_addr, ip6, INET6_ADDRSTRLEN));
  return incoming_teid;
} //CU to AMF and CU to DU then updated after add DRB 


int gtpv1u_create_s1u_tunnel(instance_t instance,
                             const gtpv1u_enb_create_tunnel_req_t  *create_tunnel_req,
                             gtpv1u_enb_create_tunnel_resp_t *create_tunnel_resp,
                             gtpCallback callBack)
{
  LOG_D(GTPU, "[%ld] Start create tunnels for UE ID %u, num_tunnels %d, sgw_S1u_teid %x\n",
        instance,
        create_tunnel_req->rnti,
        create_tunnel_req->num_tunnels,
        create_tunnel_req->sgw_S1u_teid[0]);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  
  tcp_udp_port_t dstport=inst->get_dstport();
  uint8_t addr[inst->foundAddrLen];
  memcpy(addr, inst->foundAddr, inst->foundAddrLen);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  
  for (int i = 0; i < create_tunnel_req->num_tunnels; i++) {
    AssertFatal(create_tunnel_req->eps_bearer_id[i] > 4,
                "From legacy code not clear, seems impossible (bearer=%d)\n",
                create_tunnel_req->eps_bearer_id[i]);
    int incoming_rb_id=create_tunnel_req->eps_bearer_id[i]-4;
    teid_t teid = newGtpuCreateTunnel(compatInst(instance),
                                      create_tunnel_req->rnti,
                                      incoming_rb_id,
                                      create_tunnel_req->eps_bearer_id[i],
                                      create_tunnel_req->sgw_S1u_teid[i],
                                      -1, // no pdu session in 4G
                                      create_tunnel_req->sgw_addr[i],
                                      dstport,
                                      callBack,
                                      NULL);
    create_tunnel_resp->status=0;
    create_tunnel_resp->rnti=create_tunnel_req->rnti;
    create_tunnel_resp->num_tunnels=create_tunnel_req->num_tunnels;
    create_tunnel_resp->enb_S1u_teid[i]=teid;
    create_tunnel_resp->eps_bearer_id[i] = create_tunnel_req->eps_bearer_id[i];
    memcpy(create_tunnel_resp->enb_addr.buffer,addr,sizeof(addr));
    create_tunnel_resp->enb_addr.length= sizeof(addr);
  }

  return !GTPNOK;
}

int gtpv1u_update_s1u_tunnel(
  const instance_t                              instance,
  const gtpv1u_enb_create_tunnel_req_t *const   create_tunnel_req,
  const rnti_t                                  prior_rnti
) {
  LOG_D(GTPU, "[%ld] Start update tunnels for old RNTI %x, new RNTI %x, num_tunnels %d, sgw_S1u_teid %x, eps_bearer_id %x\n",
        instance,
        prior_rnti,
        create_tunnel_req->rnti,
        create_tunnel_req->num_tunnels,
        create_tunnel_req->sgw_S1u_teid[0],
        create_tunnel_req->eps_bearer_id[0]);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));

  if ( inst->ue2te_mapping.find(create_tunnel_req->rnti) == inst->ue2te_mapping.end() ) {
    LOG_E(GTPU,"[%ld] Update not already existing tunnel (new rnti %x, old rnti %x)\n", 
          instance, create_tunnel_req->rnti, prior_rnti);
  }

  auto it=inst->ue2te_mapping.find(prior_rnti);

  if ( it != inst->ue2te_mapping.end() ) {
    pthread_mutex_unlock(&globGtp.gtp_lock);
    AssertFatal(false, "logic bug: update of non-existing tunnel (new ue id %u, old ue id %u)\n", create_tunnel_req->rnti, prior_rnti);
    /* we don't know if we need 4G or 5G PDCP and can therefore not create a
     * new tunnel */
    return 0;
  }

  inst->ue2te_mapping[create_tunnel_req->rnti]=it->second;
  inst->ue2te_mapping.erase(it);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  return 0;
}

int gtpv1u_create_ngu_tunnel(const instance_t instance,
                             const gtpv1u_gnb_create_tunnel_req_t *const create_tunnel_req,
                             gtpv1u_gnb_create_tunnel_resp_t *const create_tunnel_resp,
                             gtpCallback callBack,
                             gtpCallbackSDAP callBackSDAP)
{
  LOG_D(GTPU, "[%ld] Start create tunnels for ue id %lu, num_tunnels %d, sgw_S1u_teid %x\n",
        instance,
        create_tunnel_req->ue_id,
        create_tunnel_req->num_tunnels,
        create_tunnel_req->outgoing_teid[0]);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));

  tcp_udp_port_t dstport = inst->get_dstport();
  uint8_t addr[inst->foundAddrLen];
  memcpy(addr, inst->foundAddr, inst->foundAddrLen);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  for (int i = 0; i < create_tunnel_req->num_tunnels; i++) {
    teid_t teid = newGtpuCreateTunnel(instance,
                                      create_tunnel_req->ue_id,
                                      create_tunnel_req->incoming_rb_id[i],
                                      create_tunnel_req->pdusession_id[i],
                                      create_tunnel_req->outgoing_teid[i],
                                      create_tunnel_req->outgoing_qfi[i],
                                      create_tunnel_req->dst_addr[i],
                                      dstport,
                                      callBack,
                                      callBackSDAP);
    create_tunnel_resp->status=0;
    create_tunnel_resp->ue_id=create_tunnel_req->ue_id;
    create_tunnel_resp->num_tunnels=create_tunnel_req->num_tunnels;
    create_tunnel_resp->gnb_NGu_teid[i]=teid;
    memcpy(create_tunnel_resp->gnb_addr.buffer,addr,sizeof(addr));
    create_tunnel_resp->gnb_addr.length= sizeof(addr);
    create_tunnel_resp->pdusession_id[i] = create_tunnel_req->pdusession_id[i];
  }

  return !GTPNOK;
}

//
int gtpv1u_update_ue_id(const instance_t instanceP, ue_id_t old_ue_id, ue_id_t new_ue_id)
{
  pthread_mutex_lock(&globGtp.gtp_lock);

  auto inst = &globGtp.instances[compatInst(instanceP)];
  auto it = inst->ue2te_mapping.find(old_ue_id);
  if (it == inst->ue2te_mapping.end()) {
    LOG_W(GTPU, "[%ld] Update GTP tunnels for UEid: %lx, but no tunnel exits\n", instanceP, old_ue_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }

  for (unsigned i = 0; i < it->second.bearers.size(); ++i) {
    teid_t incoming_teid = inst->ue2te_mapping[old_ue_id].bearers[i].teid_incoming;
    if (globGtp.te2ue_mapping[incoming_teid].ue_id == old_ue_id) {
      globGtp.te2ue_mapping[incoming_teid].ue_id = new_ue_id;
    }
  }

  inst->ue2te_mapping[new_ue_id] = it->second;
  inst->ue2te_mapping.erase(it);

  pthread_mutex_unlock(&globGtp.gtp_lock);

  LOG_I(GTPU, "[%ld] Updated tunnels from UEid %lx to UEid %lx\n", instanceP, old_ue_id, new_ue_id);
  return !GTPNOK;
}

int gtpv1u_create_x2u_tunnel(
  const instance_t instanceP,
  const gtpv1u_enb_create_x2u_tunnel_req_t   *const create_tunnel_req_pP,
  gtpv1u_enb_create_x2u_tunnel_resp_t *const create_tunnel_resp_pP) {
  AssertFatal( false, "to be developped\n");
}


int newGtpuDeleteOneTunnel(instance_t instance, ue_id_t ue_id, int rb_id)
{
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  map<uint64_t, teidData_t>::iterator ue_it = inst->ue2te_mapping.find(ue_id);
  if (ue_it == inst->ue2te_mapping.end()) {
    LOG_E(GTPU, "%s() no such UE %ld\n", __func__, ue_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return !GTPNOK;
  }
  map<ue_id_t, gtpv1u_bearer_t>::iterator rb_it = ue_it->second.bearers.find(rb_id);
  if (rb_it == ue_it->second.bearers.end()) {
    LOG_E(GTPU, "%s() UE %ld has no bearer %d, available\n", __func__, ue_id, rb_id);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return !GTPNOK;
  }
  int teid = rb_it->second.teid_incoming;
  globGtp.te2ue_mapping.erase(teid);
  ue_it->second.bearers.erase(rb_id);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "Deleted tunnel TEID %d (RB %d) for ue id %ld, remaining bearers:\n", teid, rb_id, ue_id);
  for (auto b: ue_it->second.bearers)
    LOG_I(GTPU, "bearer %ld\n", b.first);
  return !GTPNOK;
}

int newGtpuDeleteAllTunnels(instance_t instance, ue_id_t ue_id) {
  LOG_D(GTPU, "[%ld] Start delete tunnels for ue id %lu\n",
        instance, ue_id);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  getUeRetInt(inst, ue_id);

  int nb=0;

  for (auto j=ptrUe->second.bearers.begin();
       j!=ptrUe->second.bearers.end();
       ++j) {
    globGtp.te2ue_mapping.erase(j->second.teid_incoming);
    nb++;
  }

  inst->ue2te_mapping.erase(ptrUe);
  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "[%ld] Deleted all tunnels for ue id %ld (%d tunnels deleted)\n", instance, ue_id, nb);
  return !GTPNOK;
}

int gtpv1u_delete_s1u_tunnel( const instance_t instance,
                              const gtpv1u_enb_delete_tunnel_req_t *const req_pP) {
  LOG_D(GTPU, "[%ld] Start delete tunnels for RNTI %x\n", instance, req_pP->rnti);
  pthread_mutex_lock(&globGtp.gtp_lock);
  auto inst = &globGtp.instances[compatInst(instance)];
  auto ptrRNTI = inst->ue2te_mapping.find(req_pP->rnti);
  if (ptrRNTI == inst->ue2te_mapping.end()) {
    LOG_W(GTPU, "[%ld] Delete Released GTP tunnels for rnti: %x, but no tunnel exits\n", instance, req_pP->rnti);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return -1;
  }

  int nb = 0;

  for (int i = 0; i < req_pP->num_erab; i++) {
    auto ptr2 = ptrRNTI->second.bearers.find(req_pP->eps_bearer_id[i]);
    if (ptr2 == ptrRNTI->second.bearers.end()) {
      LOG_E(GTPU,
            "[%ld] GTP-U instance: delete of not existing tunnel RNTI:RAB: %x/%x\n",
            instance,
            req_pP->rnti,
            req_pP->eps_bearer_id[i]);
    } else {
      globGtp.te2ue_mapping.erase(ptr2->second.teid_incoming);
      nb++;
    }
  }

  if (ptrRNTI->second.bearers.size() == 0)
    // no tunnels on this rnti, erase the ue entry
    inst->ue2te_mapping.erase(ptrRNTI);

  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "[%ld] Deleted released tunnels for RNTI %x (%d tunnels deleted)\n", instance, req_pP->rnti, nb);
  return !GTPNOK;
}

// Legacy delete tunnel finish by deleting all the ue id
int gtpv1u_delete_all_s1u_tunnel(const instance_t instance, const rnti_t rnti)
{
  return newGtpuDeleteAllTunnels(instance, rnti);
}

int newGtpuDeleteTunnels(instance_t instance, ue_id_t ue_id, int nbTunnels, pdusessionid_t *pdusession_id) {
  LOG_D(GTPU, "[%ld] Start delete tunnels for ue id %lu\n",
        instance, ue_id);
  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(compatInst(instance));
  getUeRetInt(inst, ue_id);
  int nb=0;

  for (int i=0; i<nbTunnels; i++) {
    auto ptr2=ptrUe->second.bearers.find(pdusession_id[i]);

    if ( ptr2 == ptrUe->second.bearers.end() ) {
      LOG_E(GTPU,"[%ld] GTP-U instance: delete of not existing tunnel UE ID:RAB: %ld/%x\n", instance, ue_id, pdusession_id[i]);
    } else {
      globGtp.te2ue_mapping.erase(ptr2->second.teid_incoming);
      nb++;
    }
  }

  if (ptrUe->second.bearers.size() == 0 )
    // no tunnels on this ue id, erase the ue entry
    inst->ue2te_mapping.erase(ptrUe);

  pthread_mutex_unlock(&globGtp.gtp_lock);
  LOG_I(GTPU, "[%ld] Deleted all tunnels for ue id %lu (%d tunnels deleted)\n", instance, ue_id, nb);
  return !GTPNOK;
}

int gtpv1u_delete_x2u_tunnel( const instance_t instanceP,
                              const gtpv1u_enb_delete_tunnel_req_t *const req_pP) {
  LOG_E(GTPU,"x2 tunnel not implemented\n");
  return 0;
}


int gtpv1u_delete_ngu_tunnel( const instance_t instance,
                              gtpv1u_gnb_delete_tunnel_req_t *req) {
  return  newGtpuDeleteTunnels(instance, req->ue_id, req->num_pdusession, req->pdusession_id);
}

static int Gtpv1uHandleEchoReq(int h,
                               uint8_t *msgBuf,
                               uint32_t msgBufLen,
                               uint16_t peerPort,
                               uint32_t peerIp) {
  Gtpv1uMsgHeaderT      *msgHdr = (Gtpv1uMsgHeaderT *) msgBuf;

  if ( msgHdr->version != 1 ||  msgHdr->PT != 1 ) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  if ( msgHdr->S != 1 ) {
    LOG_E(GTPU, "[%d] Received a echo request packet with no sequence number \n", h);
    return GTPNOK;
  }

  uint16_t seq=ntohs(*(uint16_t *)(msgHdr+1));
  LOG_D(GTPU, "[%d] Received a echo request, TEID: %d, seq: %hu\n", h, msgHdr->teid, seq);
  uint8_t recovery[2]= {14,0};
  return gtpv1uCreateAndSendMsg(h, peerIp, peerPort, GTP_ECHO_RSP, ntohl(msgHdr->teid), recovery, sizeof recovery, true, false, seq, 0, NO_MORE_EXT_HDRS, NULL, 0);
}

static int Gtpv1uHandleError(int h,
                             uint8_t *msgBuf,
                             uint32_t msgBufLen,
                             uint16_t peerPort,
                             uint32_t peerIp) {
  LOG_E(GTPU, "Received GTP error indication (error handling is missing/not implemented)\n");
  int rc = GTPNOK;
  return rc;
}

static int Gtpv1uHandleSupportedExt(int h,
                                    uint8_t *msgBuf,
                                    uint32_t msgBufLen,
                                    uint16_t peerPort,
                                    uint32_t peerIp) {
  LOG_E(GTPU,"Supported extensions to be dev\n");
  int rc = GTPNOK;
  return rc;
}

// When end marker arrives, we notify the client with buffer size = 0
// The client will likely call "delete tunnel"
// nevertheless we don't take the initiative
static int Gtpv1uHandleEndMarker(int h,
                                 uint8_t *msgBuf,
                                 uint32_t msgBufLen,
                                 uint16_t peerPort,
                                 uint32_t peerIp) {
  Gtpv1uMsgHeaderT      *msgHdr = (Gtpv1uMsgHeaderT *) msgBuf;

  if ( msgHdr->version != 1 ||  msgHdr->PT != 1 ) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  pthread_mutex_lock(&globGtp.gtp_lock);
  // the socket Linux file handler is the instance id
  getInstRetInt(h);

  auto tunnel = globGtp.te2ue_mapping.find(ntohl(msgHdr->teid));

  if (tunnel == globGtp.te2ue_mapping.end()) {
    LOG_E(GTPU,"[%d] Received a incoming packet on unknown teid (%x) Dropping!\n", h, msgHdr->teid);
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }

  // This context is not good for gtp
  // frame, ... has no meaning
  // manyother attributes may come from create tunnel
  protocol_ctxt_t ctxt;
  ctxt.module_id = 0;
  ctxt.enb_flag = 1;
  ctxt.instance = inst->addr.originInstance;
  ctxt.rntiMaybeUEid = tunnel->second.ue_id;
  ctxt.frame = 0;
  ctxt.subframe = 0;
  ctxt.eNB_index = 0;
  ctxt.brOption = 0;
  const srb_flag_t     srb_flag=SRB_FLAG_NO;
  const rb_id_t        rb_id=tunnel->second.incoming_rb_id;
  const mui_t          mui=RLC_MUI_UNDEFINED;
  const confirm_t      confirm=RLC_SDU_CONFIRM_NO;
  const pdcp_transmission_mode_t mode=PDCP_TRANSMISSION_MODE_DATA;
  const uint32_t sourceL2Id=0;
  const uint32_t destinationL2Id=0;
  pthread_mutex_unlock(&globGtp.gtp_lock);

  if ( !tunnel->second.callBack(&ctxt,
                                srb_flag,
                                rb_id,
                                mui,
                                confirm,
                                0,
                                NULL,
                                mode,
                                &sourceL2Id,
                                &destinationL2Id) )
    LOG_E(GTPU,"[%d] down layer refused incoming packet\n", h);

  LOG_D(GTPU,"[%d] Received END marker packet for: teid:%x\n", h, ntohl(msgHdr->teid));
  return !GTPNOK;
}
///////////////////////////////////
/////////edited by zyzy////////////
///////////////////////////////////
/*
void handlePPPoE(PPPoEPacket *pppoePkt) {
    if (!pppoePkt) {
        LOG_E(GTPU, "PPPoE packet is NULL, skipping...\n");
        return;
    }

    switch (pppoePkt->code) {
        case CODE_PADI:
            LOG_I(GTPU, "Received PADI (PPPoE Active Discovery Initiation) packet.\n");
            sendPADO(pppoePkt);
            break;

        case CODE_PADR:
            LOG_I(GTPU, "Received PADR (PPPoE Active Discovery Request) packet.\n");
            sendPADS(pppoePkt);
            break;

        case CODE_PADT:
            LOG_I(GTPU, "Received PADT (PPPoE Active Discovery Terminate) packet.\n");
            closePPPoESession(pppoePkt);
            break;

        case CODE_SESS:
            LOG_I(GTPU, "Received PPPoE Session packet.\n");
            processPPPoESession(pppoePkt);
            break;

        default:
            LOG_W(GTPU, "Unknown PPPoE packet code: %d\n", pppoePkt->code);
            break;
    }
}

static int Gtpv1uHandleGpdu(int h,
                            uint8_t *msgBuf,
                            uint32_t msgBufLen,
                            uint16_t peerPort,
                            uint32_t peerIp) {
  Gtpv1uMsgHeaderT *msgHdr = (Gtpv1uMsgHeaderT *) msgBuf;

  if (msgHdr->version != 1 || msgHdr->PT != 1) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  pthread_mutex_lock(&globGtp.gtp_lock);
  getInstRetInt(h);
  auto tunnel = globGtp.te2ue_mapping.find(ntohl(msgHdr->teid));

  if (tunnel == globGtp.te2ue_mapping.end()) {
    LOG_E(GTPU,"[%d] Received a incoming packet on unknown teid (%x) Dropping!\n", h, ntohl(msgHdr->teid));
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }

  uint8_t *payload = msgBuf + sizeof(Gtpv1uMsgHeaderT);
  uint16_t ethType = ntohs(*(uint16_t *)(payload + 12)); // Ambil Ethernet Type

  if (ethType == ETH_PPPOE_SESSION || ethType == ETH_PPPOE_DISCOVERY) {
    LOG_I(GTPU, "[%d] PPPoE packet detected in GTP-U tunnel\n", h);

    // Parsing PPPoE
    PPPoEPacket *pppoePkt = (PPPoEPacket *) payload;
    handlePPPoE(pppoePkt);
  } else {
    LOG_I(GTPU, "[%d] Non-PPPoE packet in GTP-U, processing normally\n", h);
  }

  pthread_mutex_unlock(&globGtp.gtp_lock);
  return !GTPNOK;
}
*/
//////////////////////////////////////////

static int Gtpv1uHandleGpdu(int h,
                            uint8_t *msgBuf,
                            uint32_t msgBufLen,
                            uint16_t peerPort,
                            uint32_t peerIp) {
  Gtpv1uMsgHeaderT      *msgHdr = (Gtpv1uMsgHeaderT *) msgBuf;

  if ( msgHdr->version != 1 ||  msgHdr->PT != 1 ) {
    LOG_E(GTPU, "[%d] Received a packet that is not GTP header\n", h);
    return GTPNOK;
  }

  pthread_mutex_lock(&globGtp.gtp_lock);
  // the socket Linux file handler is the instance id
  getInstRetInt(h);
  auto tunnel = globGtp.te2ue_mapping.find(ntohl(msgHdr->teid));

  if (tunnel == globGtp.te2ue_mapping.end()) {
    LOG_E(GTPU,"[%d] Received a incoming packet on unknown teid (%x) Dropping!\n", h, ntohl(msgHdr->teid));
    pthread_mutex_unlock(&globGtp.gtp_lock);
    return GTPNOK;
  }

  /* see TS 29.281 5.1 */
  //Minimum length of GTP-U header if non of the optional fields are present
  
  unsigned int offset = sizeof(Gtpv1uMsgHeaderT);

  int8_t qfi = -1;
  bool rqi = false;
  uint32_t NR_PDCP_PDU_SN = 0;

  /* if E, S, or PN is set then there are 4 more bytes of header*/
  if( msgHdr->E ||  msgHdr->S ||msgHdr->PN)
    offset += 4;

  if (msgHdr->E) {
    int next_extension_header_type = msgBuf[offset - 1];
    int extension_header_length;

    while (next_extension_header_type != NO_MORE_EXT_HDRS) {
      extension_header_length = msgBuf[offset];
      switch (next_extension_header_type) {
        case PDU_SESSION_CONTAINER: {
	  if (offset + sizeof(PDUSessionContainerT) > msgBufLen ) {
	    LOG_E(GTPU, "gtp-u received header is malformed, ignore gtp packet\n");
	    return GTPNOK;
	  }
          PDUSessionContainerT *pdusession_cntr = (PDUSessionContainerT *)(msgBuf + offset + 1);
          qfi = pdusession_cntr->QFI;
          rqi = pdusession_cntr->Reflective_QoS_activation;
          break;
        }
        case NR_RAN_CONTAINER: {
	  if (offset + 1 > msgBufLen ) {
	    LOG_E(GTPU, "gtp-u received header is malformed, ignore gtp packet\n");
	    return GTPNOK;
	  }
          uint8_t PDU_type = (msgBuf[offset+1]>>4) & 0x0f;
          if (PDU_type == 0){ //DL USER Data Format
            int additional_offset = 6; //Additional offset capturing the first non-mandatory octet (TS 38.425, Figure 5.5.2.1-1)
            if(msgBuf[offset+1]>>2 & 0x1){ //DL Discard Blocks flag is present
              LOG_I(GTPU, "DL User Data: DL Discard Blocks handling not enabled\n"); 
              additional_offset = additional_offset + 9; //For the moment ignore
            }
            if(msgBuf[offset+1]>>1 & 0x1){ //DL Flush flag is present
              LOG_I(GTPU, "DL User Data: DL Flush handling not enabled\n");
              additional_offset = additional_offset + 3; //For the moment ignore
            }
             
            if((msgBuf[offset+2]>>3)& 0x1){ //"Report delivered" enabled (TS 38.425, 5.4)
             
              /*Store the NR PDCP PDU SN for which a delivery status report shall be generated once the
               *PDU gets forwarded to the lower layers*/
              //NR_PDCP_PDU_SN = msgBuf[offset+6] << 16 | msgBuf[offset+7] << 8 | msgBuf[offset+8];

              NR_PDCP_PDU_SN = msgBuf[offset+additional_offset] << 16 | msgBuf[offset+additional_offset+1] << 8 | msgBuf[offset+additional_offset+2]; 
              LOG_D(GTPU, " NR_PDCP_PDU_SN: %u \n",  NR_PDCP_PDU_SN);
            }
          }
          else{
            LOG_W(GTPU, "NR-RAN container type: %d not supported \n", PDU_type);
          }
          break;
        }
        default:
          LOG_W(GTPU, "unhandled extension 0x%2.2x, skipping\n", next_extension_header_type);
	  break;
      }

      offset += extension_header_length * EXT_HDR_LNTH_OCTET_UNITS;
      if (offset > msgBufLen ) {
	LOG_E(GTPU, "gtp-u received header is malformed, ignore gtp packet\n");
	return GTPNOK;
      }
      next_extension_header_type = msgBuf[offset - 1];
    }
  }

  // This context is not good for gtp
  // frame, ... has no meaning
  // manyother attributes may come from create tunnel
  protocol_ctxt_t ctxt;
  ctxt.module_id = 0;
  ctxt.enb_flag = 1;
  ctxt.instance = inst->addr.originInstance;
  ctxt.rntiMaybeUEid = tunnel->second.ue_id;
  ctxt.frame = 0;
  ctxt.subframe = 0;
  ctxt.eNB_index = 0;
  ctxt.brOption = 0;
  const srb_flag_t     srb_flag=SRB_FLAG_NO;
  const rb_id_t        rb_id=tunnel->second.incoming_rb_id;
  const mui_t          mui=RLC_MUI_UNDEFINED;
  const confirm_t      confirm=RLC_SDU_CONFIRM_NO;
  const sdu_size_t sdu_buffer_size = msgBufLen - offset;
  unsigned char *const sdu_buffer=msgBuf+offset;
  const pdcp_transmission_mode_t mode=PDCP_TRANSMISSION_MODE_DATA;
  const uint32_t sourceL2Id=0;
  const uint32_t destinationL2Id=0;
  pthread_mutex_unlock(&globGtp.gtp_lock);

  if (sdu_buffer_size > 0) {
    if (qfi != -1 && tunnel->second.callBackSDAP) {
      if ( !tunnel->second.callBackSDAP(&ctxt,
                                        tunnel->second.ue_id,
                                        srb_flag,
                                        rb_id,
                                        mui,
                                        confirm,
                                        sdu_buffer_size,
                                        sdu_buffer,
                                        mode,
                                        &sourceL2Id,
                                        &destinationL2Id,
                                        qfi,
                                        rqi,
                                        tunnel->second.pdusession_id) )
        LOG_E(GTPU,"[%d] down layer refused incoming packet\n", h);
    } else {
      if ( !tunnel->second.callBack(&ctxt,
                                    srb_flag,
                                    rb_id,
                                    mui,
                                    confirm,
                                    sdu_buffer_size,
                                    sdu_buffer,
                                    mode,
                                    &sourceL2Id,
                                    &destinationL2Id) )
        LOG_E(GTPU,"[%d] down layer refused incoming packet\n", h);
    }
  }

  if(NR_PDCP_PDU_SN > 0 && NR_PDCP_PDU_SN %5 ==0){
    LOG_D (GTPU, "Create and send DL DATA Delivery status for the previously received PDU, NR_PDCP_PDU_SN: %u \n", NR_PDCP_PDU_SN);
    int rlc_tx_buffer_space = nr_rlc_get_available_tx_space(ctxt.rntiMaybeUEid, rb_id + 3);
    LOG_D(GTPU, "Available buffer size in RLC for Tx: %d \n", rlc_tx_buffer_space);
  
    /*Total size of DDD_status PDU = 1 octet to report extension header length
     * size of mandatory part + 3 octets for highest transmitted/delivered PDCP SN
     * 1 octet for padding + 1 octet for next extension header type,
     * according to TS 38.425: Fig. 5.5.2.2-1 and section 5.5.3.24*/
    
    extensionHeader_t *extensionHeader;
    extensionHeader = (extensionHeader_t *) calloc(1, sizeof(extensionHeader_t)) ;
    extensionHeader->buffer[0] = (1+sizeof(DlDataDeliveryStatus_flagsT)+3+1+1)/4;
    DlDataDeliveryStatus_flagsT DlDataDeliveryStatus;
    DlDataDeliveryStatus.deliveredPdcpSn = 0;
    DlDataDeliveryStatus.transmittedPdcpSn= 1; 
    DlDataDeliveryStatus.pduType = 1;
    DlDataDeliveryStatus.drbBufferSize = htonl(rlc_tx_buffer_space); //htonl(10000000); //hardcoded for now but normally we should extract it from RLC
    memcpy(extensionHeader->buffer+1, &DlDataDeliveryStatus, sizeof(DlDataDeliveryStatus_flagsT));
    uint8_t offset = sizeof(DlDataDeliveryStatus_flagsT)+1;

    extensionHeader->buffer[offset] =   (NR_PDCP_PDU_SN >> 16) & 0xff;
    extensionHeader->buffer[offset+1] = (NR_PDCP_PDU_SN >> 8) & 0xff;
    extensionHeader->buffer[offset+2] = NR_PDCP_PDU_SN & 0xff;
    LOG_D(GTPU, "Octets reporting NR_PDCP_PDU_SN, extensionHeader-> %u:%u:%u \n",
          extensionHeader->buffer[offset],
          extensionHeader->buffer[offset+1],
          extensionHeader->buffer[offset+2]);
    extensionHeader->buffer[offset+3] = 0x00; //Padding octet
    extensionHeader->buffer[offset+4] = 0x00; //No more extension headers
    
    /*Total size of DDD_status PDU = size of mandatory part +
     * 3 octets for highest transmitted/delivered PDCP SN +
     * 1 octet for padding + 1 octet for next extension header type,
     * according to TS 38.425: Fig. 5.5.2.2-1 and section 5.5.3.24*/
    
    extensionHeader->length  = 1+sizeof(DlDataDeliveryStatus_flagsT)+3+1+1;
    gtpv1uCreateAndSendMsg(
        h, peerIp, peerPort, GTP_GPDU, globGtp.te2ue_mapping[ntohl(msgHdr->teid)].outgoing_teid, NULL, 0, false, false, 0, 0, NR_RAN_CONTAINER, extensionHeader->buffer, extensionHeader->length);
  }

  LOG_D(GTPU,"[%d] Received a %d bytes packet for: teid:%x\n", h,
        msgBufLen-offset,
        ntohl(msgHdr->teid));
  return !GTPNOK;
}

////////////////////////////////////////////////////////////////////
void gtpv1uReceiver(int h) {
  uint8_t           udpData[65536];
  int               udpDataLen;
  socklen_t          from_len;
  struct sockaddr_in addr;
  from_len = (socklen_t)sizeof(struct sockaddr_in);

  if ((udpDataLen = recvfrom(h, udpData, sizeof(udpData), 0,
                             (struct sockaddr *)&addr, &from_len)) < 0) {
    LOG_E(GTPU, "[%d] Recvfrom failed (%s)\n", h, strerror(errno));
    return;
  } else if (udpDataLen == 0) {
    LOG_W(GTPU, "[%d] Recvfrom returned 0\n", h);
    return;
  } else {
    if ( udpDataLen < (int)sizeof(Gtpv1uMsgHeaderT) ) {
      LOG_W(GTPU, "[%d] received malformed gtp packet \n", h);
      return;
    }
    Gtpv1uMsgHeaderT* msg=(Gtpv1uMsgHeaderT*) udpData;
    if ( (int)(ntohs(msg->msgLength) + sizeof(Gtpv1uMsgHeaderT)) != udpDataLen ) {
      LOG_W(GTPU, "[%d] received malformed gtp packet length\n", h);
      return;
    }
    LOG_D(GTPU, "[%d] Received GTP data, msg type: %x\n", h, msg->msgType);
    switch(msg->msgType) {
      case GTP_ECHO_RSP:
        break;

      case GTP_ECHO_REQ:
        Gtpv1uHandleEchoReq( h, udpData, udpDataLen, htons(addr.sin_port), addr.sin_addr.s_addr);
        break;

      case GTP_ERROR_INDICATION:
        Gtpv1uHandleError( h, udpData, udpDataLen, htons(addr.sin_port), addr.sin_addr.s_addr);
        break;

      case GTP_SUPPORTED_EXTENSION_HEADER_INDICATION:
        Gtpv1uHandleSupportedExt( h, udpData, udpDataLen, htons(addr.sin_port), addr.sin_addr.s_addr);
        break;

      case GTP_END_MARKER:
        Gtpv1uHandleEndMarker( h, udpData, udpDataLen, htons(addr.sin_port), addr.sin_addr.s_addr);
        break;

      case GTP_GPDU:
        Gtpv1uHandleGpdu( h, udpData, udpDataLen, htons(addr.sin_port), addr.sin_addr.s_addr);
        break;

      default:
        LOG_E(GTPU, "[%d] Received a GTP packet of unknown type: %d\n", h, msg->msgType);
        break;
    }
  }
}

#include <openair2/ENB_APP/enb_paramdef.h>

void *gtpv1uTask(void *args)  {
  while(1) {
    /* Trying to fetch a message from the message queue.
       If the queue is empty, this function will block till a
       message is sent to the task.
    */
    MessageDef *message_p = NULL;
    itti_receive_msg(TASK_GTPV1_U, &message_p);

    if (message_p != NULL ) {
      openAddr_t addr= {{0}};
      const instance_t myInstance = ITTI_MSG_DESTINATION_INSTANCE(message_p);
      const int msgType = ITTI_MSG_ID(message_p);
      LOG_D(GTPU, "GTP-U received %s for instance %ld\n", messages_info[msgType].name, myInstance);
      switch (msgType) {
          // DATA TO BE SENT TO UDP

        case GTPV1U_DU_BUFFER_REPORT_REQ:{
          gtpv1uSendDlDeliveryStatus(compatInst(myInstance), &GTPV1U_DU_BUFFER_REPORT_REQ(message_p));
        }
        break;

        case TERMINATE_MESSAGE:
          break;

        case TIMER_HAS_EXPIRED:
          LOG_E(GTPU, "Received unexpected timer expired (no need of timers in this version) %s\n", ITTI_MSG_NAME(message_p));
          break;

        case GTPV1U_ENB_END_MARKER_REQ:
          gtpv1uEndTunnel(compatInst(myInstance), &GTPV1U_ENB_END_MARKER_REQ(message_p));
          itti_free(TASK_GTPV1_U, GTPV1U_ENB_END_MARKER_REQ(message_p).buffer);
          break;

        case GTPV1U_ENB_DATA_FORWARDING_REQ:
        case GTPV1U_ENB_DATA_FORWARDING_IND:
        case GTPV1U_ENB_END_MARKER_IND:
          LOG_E(GTPU, "to be developped %s\n", ITTI_MSG_NAME(message_p));
          abort();
          break;

        case GTPV1U_REQ:
          // to be dev: should be removed, to use API
          strcpy(addr.originHost, GTPV1U_REQ(message_p).localAddrStr);
          strcpy(addr.originService, GTPV1U_REQ(message_p).localPortStr);
          strcpy(addr.destinationService,addr.originService);
          AssertFatal((legacyInstanceMapping=gtpv1Init(addr))!=0,"Instance 0 reserved for legacy\n");
          break;

        default:
          LOG_E(GTPU, "Received unexpected message %s\n", ITTI_MSG_NAME(message_p));
          abort();
          break;
      }

      AssertFatal(EXIT_SUCCESS==itti_free(TASK_GTPV1_U, message_p), "Failed to free memory!\n");
    }

    struct epoll_event events[20];
    int nb_events = itti_get_events(TASK_GTPV1_U, events, 20);

    for (int i = 0; i < nb_events; i++)
      if ((events[i].events&EPOLLIN))
        gtpv1uReceiver(events[i].data.fd);
  }

  return NULL;
}

#ifdef __cplusplus
}
#endif
