#include "fixsmtpio_control.h"

static void parse_field(int *fields_seen, stralloc *value, filter_rule *rule) {
  char *s;

  (*fields_seen)++;

  if (!value->len) return;

  stralloc_0(value);
  s = (char *)alloc(value->len);
  str_copy(s, value->s);
  stralloc_copys(value, "");

  switch (*fields_seen) {
    case 1: rule->env                = s; break;
    case 2: rule->event              = s; break;
    case 3: rule->request_prepend    = s; break;
    case 4: rule->response_line_glob = s; break;
    case 5:
      if (!scan_ulong(s,&rule->exitcode))
        rule->exitcode = 777;
                                          break;
    case 6: rule->response           = s; break;
  }
}

filter_rule *parse_control_line(stralloc *line) {
  filter_rule *rule = (filter_rule *)alloc(sizeof(filter_rule));
  stralloc value = {0};
  int fields_seen = 0;
  int i;

  rule->next                = 0;

  rule->env                 = 0;
  rule->event               = 0;
  rule->request_prepend     = 0;
  rule->response_line_glob  = 0;
  rule->exitcode            = EXIT_LATER_NORMALLY;
  rule->response            = 0;

  for (i = 0; i < line->len; i++) {
    char c = line->s[i];
    if (':' == c && fields_seen < 5) parse_field(&fields_seen, &value, rule);
    else stralloc_append(&value, &c);
  }
  parse_field(&fields_seen, &value, rule);

  if (fields_seen < 6)            return 0;
  if (!rule->event)               return 0;
  if (!rule->response_line_glob)  return 0;
  if ( rule->exitcode > 255)      return 0;
  if (!rule->response)            rule->response = "";

  return rule;
}