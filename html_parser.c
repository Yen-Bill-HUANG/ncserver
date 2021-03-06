/*
 *  sandboxed HTML parser
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_error.h"

#define SEND_DESC 3
// #define RECEIVE_DESC 3

void parse(void) {
  char message[] = "hello";
  int sent;
  NaClMessageHeader header;
  NaClIOVec vec;

  vec.base = message;
  vec.length = sizeof(message);

  /* Test SendDatagram */
  header.iov = &vec;
  header.iov_length = 1;
  header.handles = NULL;
  header.handle_count = 0;
  sent = NaClSendDatagram(SEND_DESC, &header, 0);
  assert(sent == sizeof message);
  printf("Parser sent message \"%s\" (with size = %d)\n", message, sent);
}

int main(int argc, char **argv) {
  assert(argc >= 2);
  if (strcmp(argv[1], "parse") == 0) {
    parse();
    return 101;
  } else {
    printf("Unrecognised argument: \"%s\"\n", argv[1]);
  }
  return 0;
}
