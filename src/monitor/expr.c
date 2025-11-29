#include <regex.h>
#include <stdbool.h>
#include "common.h"

static bool div0_flag = false;
enum {
  TK_NOTYPE = 256, TK_EQ, TK_UNEQ,
  TK_DEC, TK_HEX,  TK_LOGIC_AND,
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {"0x[a-f0-9]+|0x[A-F0-9]+", TK_HEX},  // hexadecimal
  {"[0-9]+", TK_DEC},                   // decimal
  {"\\(", '('},         // left parenthesis
  {"\\)", ')'},         // right parenthesis
  {"\\*", '*'},         // multiply
  {"\\/", '/'},         // divide
  {"\\+", '+'},         // plus
  {"\\-", '-'},         // minus
  {"==", TK_EQ},        // equal
  {"!=", TK_UNEQ},      // unequal
  {"&&", TK_LOGIC_AND}, // logic and
  {" +", TK_NOTYPE},    // spaces
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

  for (i = 0; i < NR_REGEX; i ++) {
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

static Token tokens[1024] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;
  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // printf("match rules[%d] = \"%s\" at position %d with len %d: %.*s\n",
        //     i, rules[i].regex, position, substr_len, substr_len, substr_start);
        // printf("match at position %d\n%s\n%*s^\n", position, e, position, "");
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;

          case TK_HEX:
          case TK_DEC:
            tokens[nr_token].type = rules[i].token_type;
            if (substr_len >= 31) {
              Log("substring is too long with len: %d!", substr_len);
              assert(0);
            }
            else {
              strncpy(tokens[nr_token].str, substr_start, substr_len);
              tokens[nr_token].str[substr_len] = '\0';
              nr_token++;
            }
            break;

          default: 
            // printf("nr_token: %d\n", nr_token);
            tokens[nr_token].type = rules[i].token_type;
            tokens[nr_token].str[0] = '\0';
            nr_token++;
            break;
        }

        break;
      }
    }
    if (i == NR_REGEX) {
      // printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  return true;
}

static bool check_parentheses(int subtk_sp, int subtk_ep) {
  // printf("Enter in check_parentheses()\n");
  // creat a stacks to store status of '('
  bool left[nr_token];
  for (int j = 0; j < nr_token; j++) {
    left[j] = false;
  }
  int nr_left = 0, nr_right = 0;
  int i;

  if ( ( tokens[subtk_sp].type == '(' ) && ( tokens[subtk_ep].type == ')' )) {
    left[nr_left++] = true;
    for ( i = subtk_sp + 1; i <= subtk_ep; i++ ) {
      if ( tokens[i].type == '(' ) {
        left[nr_left++] = true;
        // printf("find a '(', number of '(' is %d.\n", nr_left);
      }
      else if ( tokens[i].type == ')' ) {
        nr_right++;
        // printf("find a ')', number of ')' is %d.\n", nr_right);
        if ( left[nr_left - 1] == true) {
          left[nr_left - 1] = false;
          nr_left--;
          nr_right--;
          // printf("match a piar of parenthese.\nnow number of '(' is %d, number of ')' is %d.\n", nr_left, nr_right);

          if ( ( nr_left == 0 && nr_right == 0 ) && ( i < subtk_ep) ) {
            // printf("not a shortest expr\n");
            // printf("check_parentheses() return FALSE\n"); 
            return false;
          }
        }
      }
    }
    if ( ( nr_left == 0 ) && ( nr_right == 0 ) ) {
      // printf("check_parentheses() return TRUE\n"); 
      return true;
    }
    else {
      // printf("check_parentheses() return FALSE\n"); 
      return false;
    }
  }
  else {
    // printf("check_parentheses() return FALSE\n"); 
    return false;
  }
}

static int get_token_priority(int token_type) {
  switch (token_type) {
    case TK_LOGIC_AND:
      return 1;
      break;

    case TK_EQ:
    case TK_UNEQ:
      return 2;
      break;
    
    case '+':
    case '-':
      return 3;
      break;
    
    case '*':
    case '/':
      return 4;
      break;

    default:
      return 10;
  }
}

static word_t subtk_eval(int subtk_sp, int subtk_ep, bool *success_p) {
    // printf("===============\n");
    // printf("Enter in subtk_eval()\nsubtk_sp = %d, subtk_ep = %d\n", subtk_sp, subtk_ep);
    word_t value = 0;

    if (subtk_sp > subtk_ep) {  // bad expr
      Log("Bad expr, subtk_sp (%d) is greater than subtk_ep (%d).", subtk_sp, subtk_ep);
      *success_p = false;
      return 0;
    }

    else if (subtk_sp == subtk_ep) {  // single token, which should be a dec-num or a hex-num or a reg
      switch (tokens[subtk_sp].type) {
        case TK_DEC:
          if ( sscanf(tokens[subtk_sp].str, FMT_UINT , &value) == 1 ) {
            // printf("number is %lu\n", value);
            *success_p = true;
            return value;
          }
          else {
              Log("sscanf() error when change tokens to a decimal number.");
              assert(0);
          }
          break;

        case TK_HEX:
          if ( sscanf(tokens[subtk_sp].str, FMT_WORD, &value) == 1 ) {
            // printf("number is %lx\n", value);
            *success_p = true;
            return value;
          }
          else {
            Log("sscanf() error when change tokens to a decimal number.");
            assert(0);
          }
          break;
        
        default:
          break;
      }
    }

    else if (check_parentheses(subtk_sp, subtk_ep) == true)  // expr is surronded by a matched pair of parenthese 
      return subtk_eval(subtk_sp + 1, subtk_ep - 1, success_p);

    else { // expr is not a shortest expr
      // printf("expr is not a shortest expr.\n");
      int op_pos = -1, op_type = -1, tk_prio = 100;
      word_t val_left = 0, val_right = 0;
      int i = 0, lv_parenthese = 0;

      for (i = subtk_sp; i <= subtk_ep; i++) {  // find main op
        // printf("nr_token = %d, i = %d\n", nr_token, i);
        if (tokens[i].type == '(') {  // main op is not in a pair of parenthese
          lv_parenthese++;
          // printf("find a '(', lv_parenthese is %d.\n", lv_parenthese);
          continue;
        }
        if (tokens[i].type == ')') {  // main op is not in a pair of parenthese
          lv_parenthese--;
          // printf("find a ')', lv_parenthese is %d.\n", lv_parenthese);
          continue;
        }

        if (lv_parenthese == 0) {
          if ( get_token_priority(tokens[i].type) <= tk_prio ) {
            tk_prio = get_token_priority(tokens[i].type);
            op_pos = i;
            op_type = tokens[i].type;
          }
        }
      }
      
      if (lv_parenthese != 0) {
        Log("Bad expr. Can't find matching parentheses\n");
        *success_p = false;
        return 0;
      }
      if (op_pos == -1 || op_type == -1) {
        Log("invalid expr\n");
        *success_p = false;
        return 0;
      }

        val_left = subtk_eval(subtk_sp, op_pos - 1, success_p);
        val_right = subtk_eval(op_pos + 1, subtk_ep, success_p);
      switch (op_type) {
        case '+': return ( val_left + val_right ); break;
        case '-': return ( val_left - val_right ); break;
        case '*': return ( val_left * val_right ); break;
        case '/':
          if (val_right)
            return ( val_left / val_right );
          else {
            div0_flag = true;
            return 0;
          }
          break;
        case TK_EQ:
          return (val_left == val_right) ? 1 : 0; break;
        case TK_UNEQ:
          return (val_left == val_right) ? 0 : 1; break;
        case TK_LOGIC_AND:
          return ( (val_left != 0) && (val_right != 0) ) ? 1 : 0; break;
        default: break;
      }
    }
    return 0;
}

word_t expr(char *e, bool *success_p) {
  div0_flag = false;
  // printf("Enter in expr()\nexpr e is:%s\n", e);
  if (!make_token(e)) {
    *success_p = false;
    Log("make_token() returns ERROR\n");
    return 0;
  }
  /* TODO: Insert codes to evaluate the expression. */
  if (div0_flag) {
    Log("Warning: This expression caused a div_0 error\n");
    return 0;
  }
  
  return subtk_eval(0, nr_token - 1, success_p);
}
