/***************************************************************************************
 * Copyright (c) 2014-2022 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// this should be enough
static char buf[65536] = {};            // 2^16 :2^x*10 + x < 65536 -> x <= 12
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format = "#include <stdio.h>\n"
                           "#include <stdlib.h>\n"
                           "#include <signal.h>\n"
                           "void sig_handler(int signal) {\n"
                           "if (signal == SIGFPE)\n"
                           "exit(1);\n"
                           "}\n"
                           "int main() {\n"
                           "  signal(SIGFPE, sig_handler);\n"
                           "  unsigned result = %s;\n"
                           "  printf(\"%%u\", result);\n"
                           "  return 0;\n"
                           "}";

void gen_space() {
  int n = rand() % 4;
  while (n--) {
    strcat(buf, " ");
  }
}

void gen_rand_expr(int depth) {
  if (depth > 10) { // Limit recursion depth
    sprintf(buf + strlen(buf), "%u", (unsigned int)rand());
    return;
  }

  switch (rand() % 3) {
  case 0: // num : 10 characters 1 token
    sprintf(buf + strlen(buf), "%u", (unsigned int)rand());
    gen_space();
    break;
  case 1: // () : 12 characters 3 tokens
    strcat(buf, "(");
    gen_rand_expr(depth + 1);
    strcat(buf, ")");
    break;
  default: // op : 21 characters 3 tokens
    gen_rand_expr(depth + 1);
    char op = "+-*/"[rand() % 4];
    sprintf(buf + strlen(buf), "%c", op);
    gen_rand_expr(depth + 1);
    break;
  }
}

int main(int argc, char *argv[]) {
  buf[0] = '\0';
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i++) {
    buf[0] = '\0';
    gen_rand_expr(0);

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    // add -Wall -Werror to avoid divide by zero
    int ret = system("gcc /tmp/.code.c -Wall -Werror -o /tmp/.expr");
    if (ret != 0)
      continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
