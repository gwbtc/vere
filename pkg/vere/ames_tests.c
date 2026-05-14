/// @file

#include "vere.h"
#include "io/ames/stun.h"

//  Test-local declarations for STUN helpers implemented in io/ames/stun.c.
void
_stun_make_response(const c3_y req_y[20], const sockaddr_in* lan_u, c3_y buf_y[40]);

c3_o
_stun_find_xor_mapped_address(c3_y* buf_y, c3_w len_w, sockaddr_in* lan_u);

/* _setup(): prepare for tests.
*/
static void
_setup(void)
{
  u3m_init(1 << 22);
  u3m_pave(c3y);
}

/* _test_ames(): spot check ames helpers
*/
static void
_test_ames(void)
{
  sockaddr_in lan_u = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(0x7f000001),
    .sin_port = htons(12345),
  };

  u3_noun     lan   = u3_ames_encode_lane(lan_u);
  sockaddr_in nal_u = u3_ames_decode_lane(u3k(lan));
  sockaddr_in nal_u2 = u3_ames_decode_lane(lan);

  if ( (lan_u.sin_addr.s_addr != nal_u.sin_addr.s_addr) ||
       (lan_u.sin_port != nal_u.sin_port) ||
       (lan_u.sin_addr.s_addr != nal_u2.sin_addr.s_addr) ||
       (lan_u.sin_port != nal_u2.sin_port) )
  {
    fprintf(stderr, "ames: lane fail\r\n");
    exit(1);
  }
}

static c3_i
_test_stun_addr_roundtrip(sockaddr_in* inn_u)
{
  c3_y req_y[20] = {0};
  c3_y rep_y[40];
  c3_i ret_i = 0;

  _stun_make_response(req_y, inn_u, rep_y);

  sockaddr_in lan_u;

  if ( c3n == _stun_find_xor_mapped_address(rep_y, sizeof(rep_y), &lan_u) ) {
    fprintf(stderr, "stun: failed to find addr in response\r\n");
    ret_i = 1;
  }
  else {
    if ( lan_u.sin_addr.s_addr != inn_u->sin_addr.s_addr ) {
      fprintf(stderr, "stun: addr mismatch %x %x\r\n",
                      ntohl(lan_u.sin_addr.s_addr),
                      ntohl(inn_u->sin_addr.s_addr));
      ret_i = 1;
    }

    if ( lan_u.sin_port != inn_u->sin_port ) {
      fprintf(stderr, "stun: port mismatch %u %u\r\n",
                      ntohs(lan_u.sin_port),
                      ntohs(inn_u->sin_port));
      ret_i = 1;
    }
  }

  return ret_i;
}

static c3_i
_test_stun(void)
{
  sockaddr_in inn_u = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = htonl(0x7f000001),
    .sin_port = htons(13337),
  };
  c3_w len_w = 256;

  while ( len_w-- ) {
    if ( _test_stun_addr_roundtrip(&inn_u) ) {
      return 1;
    }

    inn_u.sin_addr.s_addr = htonl(ntohl(inn_u.sin_addr.s_addr) + 1);
    inn_u.sin_port = htons(ntohs(inn_u.sin_port) + 1);
  }

  return 0;
}

/* main(): run all test cases.
*/
int
main(int argc, char* argv[])
{
  _setup();

  _test_ames();

  if ( _test_stun() ) {
    fprintf(stderr, "ames: stun tests failed\r\n");
  }

  //  GC
  //
  u3m_grab(u3_none);

  fprintf(stderr, "ames okeedokee\n");
  return 0;
}
