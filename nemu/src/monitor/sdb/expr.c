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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NUM,

  /* TODO: Add more token types
  TK_REG,
  TK_VAR, */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE},  // spaces
    {"\\+", '+'},       // plus
    {"==", TK_EQ},      // equal
    {"[0-9]+", TK_NUM}, // number
    {"\\-", '-'},       // minus
    {"\\*", '*'},       // multiply
    {"\\/", '/'},       // divide
    {"\\(", '('},       // left bracket
    {"\\)", ')'},       // right bracket
                        // {"\\$[a-z]+", TK_REG}, // register
                        // {"[a-zA-Z_]+", TK_VAR}, // variable
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

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

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
        case TK_NOTYPE:
          break;
        case TK_NUM:
          tokens[nr_token].type = TK_NUM;
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          nr_token++;
          break;
        case '+':
          tokens[nr_token].type = '+';
          nr_token++;
          break;
        case '-':
          tokens[nr_token].type = '-';
          nr_token++;
          break;
        case '*':
          tokens[nr_token].type = '*';
          nr_token++;
          break;
        case '/':
          tokens[nr_token].type = '/';
          nr_token++;
          break;
        case '(':
          tokens[nr_token].type = '(';
          nr_token++;
          break;
        case ')':
          tokens[nr_token].type = ')';
          nr_token++;
          break;
        case TK_EQ:
          tokens[nr_token].type = TK_EQ;
          nr_token++;
          break;
        default:
          TODO();
        }
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

static word_t eval(int p, int q, bool *status) {
  if (*status == false || (p > q)) {
    *status = false;
    return 0;
  }
  if (p == q) {
    if (tokens[p].type == TK_NUM) {
      *status = true;
      return atoi(tokens[p].str);
    } else {
      *status = false;
      return 0;
    }
  }
  int parentheses = cheak_parentheses(p, q);
  if (parentheses == 0) {
    *status = false;
    printf("Invalid expression\n");
    return 0;
  } else if (parentheses == 1) {
    return eval(p + 1, q - 1, status);
  } else {
    int op = -1; // the position of dominant operator
    int cnt = 0;
    int index = 0;
    int lowest_priority = 3;
    int *op_priority = (int *)calloc((q - p + 1), sizeof(int));
    int *op_position = (int *)calloc((q - p + 1), sizeof(int));
    for (int i = p; i <= q; i++) {
      if (tokens[i].type == TK_NUM)
        continue;
      if (tokens[i].type == '(') {
        cnt++;
        continue;
      }
      if (tokens[i].type == ')') {
        cnt--;
        continue;
      }
      if (cnt == 0) {
        op_position[index] = i;
        if (tokens[i].type == '+' || tokens[i].type == '-') {
          op_priority[index] = 1;
          lowest_priority = lowest_priority < 1 ? lowest_priority : 1;
        } else if (tokens[i].type == '*' || tokens[i].type == '/') {
          op_priority[index] = 2;
          lowest_priority = lowest_priority < 2 ? lowest_priority : 2;
        } else {
          op_priority[index] = 0;
          lowest_priority = lowest_priority < 0 ? lowest_priority : 0;
        }
        index++;
      }
    }
    if (cnt != 0) {
      *status = false;
      printf("Invalid expression\n");
      return 0;
    }
    for (int i = 0; i <= q - p; i++) {
      if (op_priority[i] == lowest_priority) {
        op = op_position[i] > op ? op_position[i] : op;
      }
    }
    // free malloced memory
    free(op_priority);
    free(op_position);
    word_t val1 = eval(p, op - 1, status);
    if (*status == false)
      return 0;
    word_t val2 = eval(op + 1, q, status);
    if (*status == false)
      return 0;
    switch (tokens[op].type) {
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
    }
  }
  *status = false;
  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  word_t result = eval(0, nr_token - 1, success);

  return *success ? result : 0;
}
