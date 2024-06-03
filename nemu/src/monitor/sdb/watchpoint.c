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
#include <cpu/difftest.h>

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  char expr[32];
  word_t value;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

static WP *new_wp() {
  if (free_ == NULL) {
    printf("No enough watchpoints left.\n");
    assert(0);
  }
  WP *wp = free_;
  free_ = free_->next;
  wp->next = head;
  head = wp;
  return wp;
}

static void free_wp(WP *wp) {
  if (wp == head) {
    head = head->next;
  } else {
    WP *p;
    for (p = head; p != NULL && p->next != wp; p = p->next)
      ;
    assert(p != NULL);
    p->next = wp->next;
  }
  wp->next = free_;
  free_ = wp;
}

void wp_display() {
  WP *p = head;
  if (!p) {
    printf("No watchpoints.\n");
    return;
  }
  printf("Num\t\t\t\tValue\n");
  for (; p != NULL; p = p->next) {
    printf("%d\t\t\t\t%s\n", p->NO, p->expr);
  }
}

void wp_watch(char *e, word_t value) {
  WP *p = new_wp();
  strcpy(p->expr, e);
  p->value = value;
  printf("Watchpoint %d: %s\n", p->NO, p->expr);
  printf("Initial value = %u\n", value);
}

void wp_delete(int n) {
  assert(n < NR_WP && n >= 0); // n is in [0, NR_WP)
  WP *p = &wp_pool[n];
  free_wp(p);
  printf("Watchpoint %d: %s deleted.\n", p->NO, p->expr);
}

void wp_check() {
  WP *p = head;
  for (; p != NULL; p = p->next) {
    bool success = true;
    word_t _new = expr(p->expr, &success);
    if (!success) {
      printf("Invalid expression: %s\n", p->expr);
      return;
    }
    if (_new != p->value) {
      extern NEMUState nemu_state;
      nemu_state.state = NEMU_STOP;
      printf("Watchpoint %d: %s\n", p->NO, p->expr);
      printf("Old value = %u\n", p->value);
      printf("New value = %u\n", _new);
      p->value = _new;
    }
  }
}