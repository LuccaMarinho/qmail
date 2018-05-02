#include "fixsmtpio-proxy.h"
#include "fixsmtpio-filter.h"

int accepted_data(stralloc *response) { return starts(response,"354 "); }

int is_entire_line(stralloc *sa) {
  return sa->len > 0 && sa->s[sa->len - 1] == '\n';
}

int could_be_final_response_line(stralloc *line) {
  return line->len >= 4 && line->s[3] == ' ';
}

int is_entire_response(stralloc *response) {
  stralloc lastline = {0};
  int pos = 0;
  int i;
  if (!is_entire_line(response)) return 0;
  for (i = response->len - 2; i >= 0; i--) {
    if (response->s[i] == '\n') {
      pos = i + 1;
      break;
    }
  }
  copyb(&lastline,response->s+pos,response->len-pos);
  return could_be_final_response_line(&lastline);
}

fd_set fds;

int max(int a,int b) { return a > b ? a : b; }

void want_to_read(int fd1,int fd2) {
  FD_ZERO(&fds);
  FD_SET(fd1,&fds);
  FD_SET(fd2,&fds);
}

int can_read(int fd) { return FD_ISSET(fd,&fds); }

int can_read_something(int fd1,int fd2) {
  int ready;
  ready = select(1+max(fd1,fd2),&fds,(fd_set *)0,(fd_set *)0,(struct timeval *) 0);
  if (ready == -1 && errno != error_intr) die_read();
  return ready;
}

int saferead(int fd,char *buf,int len) {
  int r;
  r = read(fd,buf,len);
  if (r == -1) if (errno != error_intr) die_read();
  return r;
}

int safeappend(stralloc *sa,int fd,char *buf,int len) {
  int r;
  r = saferead(fd,buf,len);
  catb(sa,buf,r);
  return r;
}

int is_last_line_of_data(stralloc *r) {
  return (r->len == 3 && r->s[0] == '.' && r->s[1] == '\r' && r->s[2] == '\n');
}

void parse_client_request(stralloc *verb,stralloc *arg,stralloc *request) {
  int i;
  for (i = 0; i < request->len; i++)
    if (request->s[i] == ' ') break;

  // XXX: Pull this behaviour out into it's own function (please )
  i++;

  // XXX: Test edge case >= vs >
  if (i > request->len) {
    copy(verb,request);
    strip_last_eol(verb);
    blank(arg);
  } else {
    copyb(verb,request->s,i-1);
    copyb(arg,request->s+i,request->len-i);
    strip_last_eol(arg);
  }
}

void safewrite(int fd,stralloc *sa) {
  if (write(fd,sa->s,sa->len) == -1) die_write();
}

void construct_proxy_request(stralloc *proxy_request,
                             filter_rule *rules,
                             stralloc *verb,stralloc *arg,
                             stralloc *client_request,
                             int *want_data,int *in_data) {
  filter_rule *rule;

  if (*in_data) {
    copy(proxy_request,client_request);
    if (is_last_line_of_data(proxy_request)) *in_data = 0;
  } else {
    for (rule = rules; rule; rule = rule->next)
      if (rule->request_prepend && filter_rule_applies(rule,verb))
        cats(proxy_request,rule->request_prepend);
    if (verb_matches("data",verb)) *want_data = 1;
    cat(proxy_request,client_request);
  }
}

void construct_proxy_response(stralloc *proxy_response,
                              stralloc *greeting,
                              filter_rule *rules,
                              stralloc *verb,stralloc *arg,
                              stralloc *server_response,
                              int request_received,
                              int *proxy_exitcode,
                              int *want_data,int *in_data) {
  if (*want_data) {
    *want_data = 0;
    if (accepted_data(server_response)) *in_data = 1;
  }
  copy(proxy_response,server_response);
  if (!*in_data && !request_received && !verb->len)
    copys(verb,PSEUDOVERB_TIMEOUT);
  munge_response(proxy_response,proxy_exitcode,greeting,rules,verb);
}

void request_response_init(request_response *rr) {
  static stralloc client_request  = {0},
                  client_verb     = {0},
                  client_arg      = {0},
                  proxy_request   = {0},
                  server_response = {0},
                  proxy_response  = {0};

  blank(&client_request);  rr->client_request  = &client_request;
  blank(&client_verb);     rr->client_verb     = &client_verb;
  blank(&client_arg);      rr->client_arg      = &client_arg;
  blank(&proxy_request);   rr->proxy_request   = &proxy_request;
  blank(&server_response); rr->server_response = &server_response;
  blank(&proxy_response);  rr->proxy_response  = &proxy_response;
                           rr->proxy_exitcode  = EXIT_LATER_NORMALLY;
}

void handle_client_eof(stralloc *line,int lineno,int *exitcode,
                       stralloc *greeting,filter_rule *rules) {
  stralloc client_eof = {0};
  copys(&client_eof,PSEUDOVERB_CLIENTEOF);
  munge_response_line(line,lineno,exitcode,greeting,rules,&client_eof);
}

void logit(char logprefix,stralloc *sa) {
  if (!env_get("FIXSMTPIODEBUG")) return;
  substdio_put(&sserr,&logprefix,1);
  substdio_puts(&sserr,": ");
  substdio_put(&sserr,sa->s,sa->len);
  if (!is_entire_line(sa)) substdio_puts(&sserr,"\r\n");
  substdio_flush(&sserr);
}

void handle_client_request(int to_server,filter_rule *rules,
                           request_response *rr,
                           int *want_data,int *in_data) {
  logit('1',rr->client_request);
  if (!*in_data)
    parse_client_request(rr->client_verb,rr->client_arg,rr->client_request);
  logit('2',rr->client_verb);
  logit('3',rr->client_arg);
  construct_proxy_request(rr->proxy_request,rules,
                          rr->client_verb,rr->client_arg,
                          rr->client_request,
                          want_data,in_data);
  logit('4',rr->proxy_request);
  safewrite(to_server,rr->proxy_request);
  if (*in_data) {
    blank(rr->client_request);
    blank(rr->proxy_request);
  }
}

int handle_server_response(int to_client,
                           stralloc *greeting,filter_rule *rules,
                           request_response *rr,
                           int *want_data,int *in_data) {
  logit('5',rr->server_response);
  construct_proxy_response(rr->proxy_response,
                           greeting,rules,
                           rr->client_verb,rr->client_arg,
                           rr->server_response,
                           rr->client_request->len,
                           &rr->proxy_exitcode,
                           want_data,in_data);
  logit('6',rr->proxy_response);
  safewrite(to_client,rr->proxy_response);
  return rr->proxy_exitcode;
}

int read_and_process_until_either_end_closes(int from_client,int to_server,
                                             int from_server,int to_client,
                                             stralloc *greeting,
                                             filter_rule *rules) {
  char buf[SUBSTDIO_INSIZE];
  int exitcode = EXIT_LATER_NORMALLY;
  int want_data = 0, in_data = 0;
  request_response rr;

  request_response_init(&rr);
  copys(rr.client_verb,PSEUDOVERB_GREETING);

  for (;;) {
    want_to_read(from_client,from_server);
    if (!can_read_something(from_client,from_server)) continue;

    if (can_read(from_client)) {
      if (!safeappend(rr.client_request,from_client,buf,sizeof buf)) {
        handle_client_eof(rr.client_request,0,&exitcode,greeting,rules);
        break;
      }
      if (is_entire_line(rr.client_request))
        handle_client_request(to_server,rules,&rr,&want_data,&in_data);
    }

    if (can_read(from_server)) {
      if (!safeappend(rr.server_response,from_server,buf,sizeof buf)) break;
      if (is_entire_response(rr.server_response)) {
        exitcode = handle_server_response(to_client,greeting,rules,&rr,&want_data,&in_data);
        request_response_init(&rr);
      }
    }

    if (exitcode != EXIT_LATER_NORMALLY) break;
  }

  return exitcode;
}
