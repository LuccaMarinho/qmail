#include "fixsmtpio_proxy.h"
#include "fixsmtpio_eventq.h"
#include "fixsmtpio_filter.h"

int accepted_data(stralloc *response) { return starts(response,"354 "); }

int find_first_newline(stralloc *sa) {
  int first_occurrence_of_newline = -1;
  int i;
  for (i = 0; i < sa->len; i++) {
    if (sa->s[i] == '\n') {
      first_occurrence_of_newline = i;
      break;
    }
  }

  return first_occurrence_of_newline;
}

int is_entire_line(stralloc *sa) {
  int is_at_least_one_line = sa->len > 0 && sa->s[sa->len - 1] == '\n';
  //int first_occurrence_of_newline = find_first_newline(sa);

  return is_at_least_one_line;
  //this throws off some callers. breaks filtering, at least. try `ehlo` to see.
  //return (is_at_least_one_line && first_occurrence_of_newline == (sa->len - 1));
}

int is_last_line_of_response(stralloc *line) {
  return line->len >= 4 && line->s[3] == ' ';
}

/*
  NULL: false
  empty: false
  non-empty but no "\r\n" at the end: false
  two lines, both ending in "\r\n", both with '-' as char 4: false
  two lines, first with ' ' as char 4, second with '-': false
  two lines, first with '-' as char 4, second with ' ': true
 */
int is_at_least_one_response(stralloc *response) {
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
  return is_last_line_of_response(&lastline);
}

void strip_last_eol(stralloc *sa) {
  if (sa->len > 0 && sa->s[sa->len-1] == '\n') sa->len--;
  if (sa->len > 0 && sa->s[sa->len-1] == '\r') sa->len--;
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

/*
  NULL: false
  empty: false
  " \r\n": false
  ".\r\n": true
 */
int is_last_line_of_data(stralloc *r) {
  return (r->len == 3 && r->s[0] == '.' && r->s[1] == '\r' && r->s[2] == '\n');
}

void parse_client_request(stralloc *verb,stralloc *arg,stralloc *request) {
  int i;
  for (i = 0; i < request->len; i++)
    if (request->s[i] == ' ') break;

  // XXX: Pull this behaviour out into it's own function (please )
  // int pos = find_first_space(request)
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

/*
  in_data: whatever client_request is, proxy_request is exactly the same
  not in_data, a rule says to prepend: proxy request is equal to prepended + client_request
  not in_data, two rules prepend: proxy request is equal to p1 + p2 + client_request (WILL FAIL)

  not sure whether to test in_data/want_data state changes
 */
void construct_proxy_request(stralloc *proxy_request,
                             filter_rule *rules,
                             char *event,stralloc *arg,
                             stralloc *client_request,
                             int *want_data,int *in_data) {
  filter_rule *rule;

  if (*in_data) {
    copy(proxy_request,client_request);
    if (is_last_line_of_data(proxy_request)) *in_data = 0;
  } else {
    for (rule = rules; rule; rule = rule->next)
      if (rule->request_prepend && filter_rule_applies(rule,event))
        cats(proxy_request,rule->request_prepend); //XXX if we have stuff already, this is not prepending!!!
    if (event_matches("data",event)) *want_data = 1;
    cat(proxy_request,client_request);
  }
}

/*
  not sure whether to test:
  not want_data, not in_data, no request_received, no verb: it's a timeout
 */
void construct_proxy_response(stralloc *proxy_response,
                              stralloc *greeting,
                              filter_rule *rules,
                              char *event,
                              stralloc *server_response,
                              int *proxy_exitcode,
                              int *want_data,int *in_data) {
  if (*want_data) {
    *want_data = 0;
    if (accepted_data(server_response)) *in_data = 1;
  }
  copy(proxy_response,server_response);
  munge_response(proxy_response,proxy_exitcode,greeting,rules,event);
}

void proxied_request_init(proxied_request *rq) {
  static stralloc client_request  = {0},
                  client_verb     = {0},
                  client_arg      = {0},
                  proxy_request   = {0};

  blank(&client_request);  rq->client_request  = &client_request;
  blank(&client_verb);     rq->client_verb     = &client_verb;
  blank(&client_arg);      rq->client_arg      = &client_arg;
  blank(&proxy_request);   rq->proxy_request   = &proxy_request;
}

void proxied_response_init(proxied_response *rp) {
  static stralloc server_response = {0},
                  proxy_response  = {0};

  blank(&server_response); rp->server_response = &server_response;
  blank(&proxy_response);  rp->proxy_response  = &proxy_response;
                           rp->proxy_exitcode  = EXIT_LATER_NORMALLY;
}

void logit(char logprefix,stralloc *sa) {
  if (!env_get("FIXSMTPIODEBUG")) return;
  substdio_put(&sserr,&logprefix,1);
  substdio_puts(&sserr,": ");
  substdio_put(&sserr,sa->s,sa->len);
  if (!is_entire_line(sa)) substdio_puts(&sserr,"\r\n");
  substdio_flush(&sserr);
}

char *handle_client_request(int to_server,filter_rule *rules,
                            proxied_request *rq,
                            int *want_data,int *in_data) {
  stralloc event_sa = {0};
  logit('1',rq->client_request);
  if (!*in_data)
    parse_client_request(rq->client_verb,rq->client_arg,rq->client_request);
  logit('2',rq->client_verb);
  copy(&event_sa,rq->client_verb);
  stralloc_0(&event_sa);
  logit('3',rq->client_arg);
  construct_proxy_request(rq->proxy_request,rules,
                          event_sa.s,rq->client_arg,
                          rq->client_request,
                          want_data,in_data);
  logit('4',rq->proxy_request);
  safewrite(to_server,rq->proxy_request);
  if (*in_data) {
    blank(rq->client_request);
    blank(rq->proxy_request);
  }

  return event_sa.s;
}

void get_one_response(stralloc *one,stralloc *pile) {
  stralloc next_pile = {0};
  int pos = 0;
  int i;

  copys(one,"");

  for (i = pos; i < pile->len; i++) {
    if (pile->s[i] == '\n') {
      stralloc line = {0};
      copys(&line,"");

      catb(&line,pile->s+pos,i+1-pos);
      pos = i+1;
      cat(one,&line);

      if (is_last_line_of_response(&line)) break;
    }
  }

  copyb(&next_pile,pile->s+pos,pile->len-pos);
  copy(pile,&next_pile);
}

int read_and_process_until_either_end_closes(int from_client,int to_server,
                                             int from_server,int to_client,
                                             stralloc *greeting,
                                             filter_rule *rules) {
  char buf[SUBSTDIO_INSIZE];
  int exitcode = EXIT_LATER_NORMALLY;
  int want_data = 0, in_data = 0;
  proxied_request rq;
  proxied_response rp;

  proxied_request_init(&rq);
  proxied_response_init(&rp);
  eventq_put(EVENT_GREETING);

  for (;;) {
    want_to_read(from_client,from_server);
    if (!can_read_something(from_client,from_server)) continue;

    if (can_read(from_client)) {
      if (!safeappend(rq.client_request,from_client,buf,sizeof buf)) {
        eventq_put(EVENT_CLIENTEOF);
        break;
      }
      if (is_entire_line(rq.client_request)) {
        char *event = handle_client_request(to_server,rules,&rq,&want_data,&in_data);
        eventq_put(event);
        alloc_free(event);
        proxied_request_init(&rq);
      }
    }

    if (can_read(from_server)) {
      if (!safeappend(rp.server_response,from_server,buf,sizeof buf)) break;
      if (is_at_least_one_response(rp.server_response)) {
        stralloc one_response = {0};
        char *event;
        while (rp.server_response->len && rp.proxy_exitcode == EXIT_LATER_NORMALLY) {
          get_one_response(&one_response,rp.server_response);
          logit('5',&one_response);
          event = eventq_get();
          construct_proxy_response(rp.proxy_response,
                                   greeting,rules,event,
                                   &one_response,
                                   &rp.proxy_exitcode,
                                   &want_data,&in_data);
          logit('6',rp.proxy_response);
          alloc_free(event);
          safewrite(to_client,rp.proxy_response);
        }
        if (exitcode != EXIT_LATER_NORMALLY) break;
        proxied_response_init(&rp);
      }
    }

    if (exitcode != EXIT_LATER_NORMALLY) break;
  }

  return exitcode;
}
