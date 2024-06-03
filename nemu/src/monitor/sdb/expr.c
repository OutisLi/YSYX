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

#include "common.h"
#include <debug.h>
#include <isa.h>
#include <memory/vaddr.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NEQ,
  TK_GT,
  TK_LT,
  TK_GE,
  TK_LE,
  TK_NUM,
  TK_POS,
  TK_NEG,
  TK_AND,
  TK_OR,
  TK_REG,
  TK_DEREF, // pointer dereference
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
    {" +", TK_NOTYPE},       // spaces
    {"\\+", '+'},            // plus
    {"==", TK_EQ},           // equal
    {"(0x)?[0-9]+", TK_NUM}, // number
    {"\\-", '-'},            // minus
    {"\\*", '*'},            // multiply
    {"\\/", '/'},            // divide
    {"\\(", '('},            // left bracket
    {"\\)", ')'},            // right bracket
    {"<", TK_LT},            // less than
    {">", TK_GT},            // greater than
    {"<=", TK_LE},           // less than or equal to
    {">=", TK_GE},           // greater than or equal to
    {"!=", TK_NEQ},          // not equal to
    {"&&", TK_AND},          // and
    {"\\|\\|", TK_OR},       // or
    {"\\$[a-z0-9]+", TK_REG},   // register
                             // {"[a-zA-Z_]+", TK_VAR}, // variable
};

static int nonop[] = {TK_NUM, '(', TK_REG, ')'};
static int hop[] = {TK_NEG, TK_POS, TK_DEREF}; // unit operator
static int speop[] = {
    TK_NUM, ')', TK_REG}; // special operator: +-* means label ifnot after these

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

#define TK_ISTYPE(type, types) tk_istype(type, types, ARRLEN(types))

// cannot obtain the size of an array in a function
static bool tk_istype(int type, int *types, int n) {
  for (int i = 0; i < n; i++) {
    if (type == types[i])
      return true;
  }
  return false;
}

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[128] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

void _print_tokens(int s, int end) {
  for (int i = s; i <= end; i++) {
    if (tokens[i].type == TK_NUM) {
      printf("%s", tokens[i].str);
    } else {
      printf("%c", tokens[i].type);
    }
  }
  printf("\n");
}

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i,
            rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (rules[i].token_type == TK_NOTYPE)
          break;
        tokens[nr_token].type = rules[i].token_type;

        switch (rules[i].token_type) {
        case TK_NUM:
        case TK_REG:
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          break;
        case '+':
          if (nr_token == 0 || !TK_ISTYPE(tokens[nr_token - 1].type, speop)) {
            tokens[nr_token].type = TK_POS;
            Log("change + to POS at position %d", position - 1);
          }
          break;
        case '-':
          if (nr_token == 0 || !TK_ISTYPE(tokens[nr_token - 1].type, speop)) {
            tokens[nr_token].type = TK_NEG;
            Log("change - to NEG at position %d", position - 1);
          }
          break;
        case '*':
          if (nr_token == 0 || !TK_ISTYPE(tokens[nr_token - 1].type, speop)) {
            tokens[nr_token].type = TK_DEREF;
            Log("change * to DEREF at position %d", position - 1);
          }
          break;
        }
        nr_token++;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  return true;
}

// return 0 if the expression is invalid (i.e. cnt != 0)
// return 1 if the expression is valid and the parentheses are matched
// return 2 if the expression is valid but the parentheses are not matched
static int cheak_parentheses(int p, int q) {
  if (tokens[p].type == '(' && tokens[q].type == ')') {
    int cnt = 0;
    // flag = 1 means the leftmost bracket is paired with a right bracket in the
    // middle
    int flag = 0;
    for (int i = p; i <= q; i++) {
      if (tokens[i].type == '(')
        cnt++;
      if (tokens[i].type == ')')
        cnt--;
      if (cnt == 0 && i < q)
        flag = 1;
    }
    return cnt == 0 ? (flag == 1 ? 2 : 1) : 0;
  }
  return 2;
}

static word_t eval_single(int p, bool *status) {
  switch (tokens[p].type) {
  case TK_NUM:
    if (strncmp(tokens[p].str, "0x", 2) == 0)
      return strtol(tokens[p].str, NULL, 16);
    else
      return atoi(tokens[p].str);
  case TK_REG:
    return isa_reg_str2val(tokens[p].str, status);
  default:
    *status = false;
    return 0;
  }
}

static word_t cal_single(int op, word_t val, bool *status) {
  switch (op) {
  case TK_POS:
    return val;
  case TK_NEG:
    return -val;
  case TK_DEREF:
    return vaddr_read(val, 4);
  default:
    *status = false;
    return 0;
  }
}

static word_t cal_two(word_t val1, int op, word_t val2, bool *status) {
  switch (op) {
  case '+':
    return val1 + val2;
  case '-':
    return val1 - val2;
  case '*':
    return val1 * val2;
  case '/':
    if (val2 == 0) {
      *status = false;
      return 0;
    }
    return (sword_t)val1 / (sword_t)val2;
  case TK_EQ:
    return val1 == val2;
  case TK_NEQ:
    return val1 != val2;
  case TK_GT:
    return val1 > val2;
  case TK_LT:
    return val1 < val2;
  case TK_GE:
    return val1 >= val2;
  case TK_LE:
    return val1 <= val2;
  case TK_AND:
    return val1 && val2;
  case TK_OR:
    return val1 || val2;
  default:
    *status = false;
    return 0;
  }
}

static word_t eval(int p, int q, bool *status) {
  if (*status == false || (p > q)) {
    *status = false;
    return 0;
  }
  if (p == q) {
    return eval_single(p, status);
  }

  int parentheses = cheak_parentheses(p, q);
  if (parentheses == 0) {
    *status = false;
    printf("Invalid parentheses\n");
    return 0;
  } else if (parentheses == 1) {
    return eval(p + 1, q - 1, status);
  } else {
    int op = -1; // the position of dominant operator
    int cnt = 0;
    int index = 0;
    int lowest_priority = 100;
    int *op_priority = (int *)calloc((q - p + 1), sizeof(int));
    int *op_position = (int *)calloc((q - p + 1), sizeof(int));
    for (int i = p; i <= q; i++) {
      if (tokens[i].type == '(') {
        cnt++;
        continue;
      }
      if (tokens[i].type == ')') {
        if (cnt == 0) {
          *status = false;
          printf("Invalid parentheses\n");
          return 0;
        }
        cnt--;
        continue;
      }
      if (TK_ISTYPE(tokens[i].type, nonop))
        continue;
      if (cnt == 0) {
        op_position[index] = i;
        if (tokens[i].type == '*' || tokens[i].type == '/') {
          op_priority[index] = 6;
          lowest_priority = lowest_priority < 6 ? lowest_priority : 6;
        } else if (tokens[i].type == '+' || tokens[i].type == '-') {
          op_priority[index] = 5;
          lowest_priority = lowest_priority < 5 ? lowest_priority : 5;
        } else if (tokens[i].type == TK_LT || tokens[i].type == TK_GT ||
                   tokens[i].type == TK_LE || tokens[i].type == TK_GE) {
          op_priority[index] = 4;
          lowest_priority = lowest_priority < 4 ? lowest_priority : 4;
        } else if (tokens[i].type == TK_EQ || tokens[i].type == TK_NEQ) {
          op_priority[index] = 3;
          lowest_priority = lowest_priority < 3 ? lowest_priority : 3;
        } else if (tokens[i].type == TK_AND) {
          op_priority[index] = 2;
          lowest_priority = lowest_priority < 2 ? lowest_priority : 2;
        } else if (tokens[i].type == TK_OR) {
          op_priority[index] = 1;
          lowest_priority = lowest_priority < 1 ? lowest_priority : 1;
        } else if (TK_ISTYPE(tokens[i].type, hop)) {
          op_priority[index] = 7;
          lowest_priority = lowest_priority < 7 ? lowest_priority : 7;
        }
        index++;
      }
    }
    if (cnt != 0) {
      *status = false;
      printf("Invalid parentheses\n");
      return 0;
    }
    for (int i = 0; i <= q - p; i++) {
      if (op_priority[i] == lowest_priority) {
        op = op_position[i] > op ? op_position[i] : op;
      }
    }
    if (op < 0 || op > q) {
      *status = false;
      printf("major op not found\n");
      return 0;
    }
    // free malloced memory
    free(op_priority);
    free(op_position);
    bool statusL = true, statusR = true;
    word_t val1 = eval(p, op - 1, &statusL);
    word_t val2 = eval(op + 1, q, &statusR);
    if (statusR == false) { // cannot 1+, but can +1
      *status = false;
      return 0;
    }
    if (statusL) {
      word_t result = cal_two(val1, tokens[op].type, val2, status);
      return result;
    } else {
      return cal_single(tokens[op].type, val2, status);
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  word_t result = eval(0, nr_token - 1, success);

  return *success ? result : 0;
}
