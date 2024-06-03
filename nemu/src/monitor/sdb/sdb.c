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

#include "sdb.h"
#include "utils.h"
#include <common.h>
#include <cpu/cpu.h>
#include <isa.h>
#include <memory/vaddr.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin.
 */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char *args) {
  char *arg = strtok(NULL, " ");
  int n = 1;
  if (arg != NULL) {
    sscanf(arg, "%d", &n);
  }
  if (n <= 0) {
    printf("Invalid argument: %d\n", n);
    return 0;
  }
  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Invalid argument\n");
    return 0;
  }
  if (strcmp(arg, "r") == 0) {
    isa_reg_display();
  } else if (strcmp(arg, "w") == 0) {
    wp_display();
  } else {
    printf("Unknown argument '%s'\n", arg);
  }
  return 0;
}

static int cmd_x(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Invalid argument\n");
    return 0;
  }
  int n;
  sscanf(arg, "%d", &n);
  arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Invalid argument\n");
    return 0;
  }
  vaddr_t addr;
  sscanf(arg, "%x", &addr);
  for (int i = 0; i < n; i++) {
    if (i % 4 == 0) {
      printf("0x%08x: ", addr);
    }
    printf("0x%08x ", vaddr_read(addr, 4));
    if (i % 4 == 3) {
      printf("\n");
    }
    addr += 4;
  }
  if (n % 4 != 0) {
    printf("\n");
  }
  return 0;
}

static int cmd_q(char *args) { return -1; }
static int cmd_p(char *args) {
  bool success = true;
  word_t result = expr(args, &success);
  if (success) {
    printf("%u\n", result);
  } else {
    printf("Invalid expression\n");
  }
  return 0;
}

static int cmd_w(char *args) {
  bool success = true;
  word_t result = expr(args, &success);
  if (success) {
    wp_watch(args, result);
  } else {
    printf("Invalid expression\n");
  }
  return 0;
}

static int cmd_d(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Invalid argument\n");
    return 0;
  }
  int n;
  sscanf(arg, "%d", &n);
  wp_delete(n);
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"q", "Exit NEMU", cmd_q},
    {"si", "Execute N instructions in a single step", cmd_si},
    {"info", "Print program status", cmd_info},
    {"x", "Examine memory", cmd_x},
    {"p", "Print value of expression", cmd_p},
    {"w", "Set a watchpoint", cmd_w},
    {"d", "Delete a watchpoint", cmd_d},
    // {"bt", "Print backtrace of all stack frames", cmd_bt},
    // {"cache", "Print cache status", cmd_cache},
    // {"tlb", "Print tlb status", cmd_tlb},
    // {"page", "Print page table status", cmd_page},
    // {"set", "Set value of register", cmd_set},
    // {"watch", "Print watchpoint status", cmd_watch},

    /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) {
          extern NEMUState nemu_state;
          nemu_state.state = NEMU_QUIT;
          return;
        }
        break;
      }
    }

    if (i == NR_CMD) {
      printf("Unknown command '%s'\n", cmd);
    }
  }
}

void test_expr() {
  FILE *fp = fopen("/home/outisli/Documents/ysyx-workbench/nemu/tools/gen-expr/"
                   "build/input.txt",
                   "r");
  assert(fp != NULL);
  char *e = NULL;
  word_t target_res;
  size_t len = 0;
  ssize_t read;
  bool success = true;
  int i = 0;

  while (true) {
    i++;
    // printf("Test case %d\n", i);
    if (fscanf(fp, "%u ", &target_res) == -1)
      break;
    read = getline(&e, &len, fp);
    e[read - 1] = '\0';

    word_t res = expr(e, &success);

    assert(success);
    if (res != target_res) {
      puts(e);
      printf("Expected result is: %u, but got: %u\n", target_res, res);
      assert(0);
    }
  }

  fclose(fp);
  if (e)
    free(e);

  Log("expression test pass");
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  // test_expr();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
