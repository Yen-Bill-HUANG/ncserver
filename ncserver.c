/*
 * This test program integrates a web server into the host process and places the HTML parser into a NaCl sandbox.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

// IMC communication between server and sandboxed HTML parser
#include "native_client/src/public/imc_types.h"
#include "native_client/src/public/imc_syscalls.h"

// our web server files
#include "native_client/src/shared/platform/conf.h"
#include "native_client/src/shared/platform/macro.h"
#include "native_client/src/shared/platform/ncwebserver.h"
#include "native_client/src/shared/platform/usage.h"

#define SEND_DESC 3

const char ncwebserver_usage_string[] = 
    "<executable file> [-h] [-d web-root] [-b bind-address] [-p port]";

static void
parse_argv(int argc, char **argv, struct ncwebserver_conf_t *conf)
{
    int c;

    while ((c = getopt(argc, argv, "b:hp:d:")) != -1) {
        switch (c) {
            case 'b':
                // Check the user has specified valid IP address.
                if (inet_pton(AF_INET, optarg, &conf->addr) == 0) {
                    fprintf(stderr, "-b: please check the bind address '%s'\n", optarg);
                    exit(1);
                }
                break;
            case 'd':
                strncpy(conf->webroot, optarg, sizeof(conf->webroot));
                break;
            case 'h':
                usage(ncwebserver_usage_string);
                break;
            case 'p':
                // Check the destination port number is a valid port number
                // such that it is in the range of [0, 65535]
                if (in_range(atoi(optarg), 0, 65535) == 0) {
                    fprintf(stderr, "error: the port number '%d' must be in the range [0, 65535].\n", atoi(optarg));
                    exit(1);
                }
                conf->port = atoi(optarg);
                break;
        }
    }
}

int main(int argc, char **argv) {
  
  struct NaClApp app;
  struct NaClDesc *nd;
  NaClHandle handle_pair[2];
  char *parse_args[] = {"prog", "parse"};
  int return_code;
  char buffer[100];
  NaClMessageHeader header;
  NaClIOVec vec;

  struct ncwebserver_t server;
  struct ncwebserver_conf_t conf;

  if (argc < 2){
    usage(ncwebserver_usage_string);
  }

  NaClAllModulesInit();
  NaClFileNameForValgrind(argv[1]);
  nd = (struct NaClDesc *) NaClDescIoDescOpen(argv[1], NACL_ABI_O_RDONLY, 0);
  CHECK(NULL != nd);
  CHECK(NaClAppCtor(&app));
  /* Use a smaller guest address space size, because 32-bit Windows
     does not let us allocate 2GB of address space.  We don't do this
     for x86-64 because there is an assertion in NaClAllocateSpace()
     that requires 4GB. */
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  app.addr_bits = 29; /* 512MB per process */
#endif
  /*
   * On x86-32, we cannot enable ASLR even when the address space is
   * 512MB.  In particular, when on Windows where the available
   * contiguous address space is tight, if the random choice for the
   * base of the first 512MB address space puts that address space
   * in the middle of the available address space region, the
   * address space allocation code might not be able to (randomly)
   * find another contiguous 512MB region for the second NaCl
   * module.
   */
  CHECK(NaClAppLoadFileAslr(nd, &app,
                            NACL_DISABLE_ASLR) == LOAD_OK);
  NaClAppInitialDescriptorHookup(&app);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);
  /* Set up an IMC connection between the host and guest. */
  CHECK(NaClSocketPair(handle_pair) == 0);
  NaClAddImcHandle(&app, handle_pair[0], SEND_DESC);
  CHECK(NaClCreateMainThread(&app, 2, parse_args, NULL));
  return_code = NaClWaitForMainThreadToExit(&app);
  CHECK(return_code == 101);

  // receive message sent from HTML parser
  vec.base = buffer;
  vec.length = sizeof buffer;
  memset(buffer, 0, sizeof buffer);
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  return_code = NaClReceiveDatagram(handle_pair[1], &header, 0);
  assert(return_code == sizeof buffer);
  printf("Server received \"%s\" (with size = %d)\n", buffer+16, return_code);

  // run web server
  memset(&conf, 0, sizeof(conf));
  parse_argv(argc, argv, &conf);
  ncwebserver_init(&server, &conf);
  ncwebserver_start(&server);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
}
