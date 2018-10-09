void assert_strip_last_eol(const char *input, const char *expected_output) {
  stralloc sa = {0}; stralloc_copys(&sa, input);

  strip_last_eol(&sa);

  ck_assert_int_eq(sa.len, strlen(expected_output));
  stralloc_0(&sa);
  ck_assert_str_eq(sa.s, expected_output);
}

START_TEST (test_strip_last_eol)
{
  assert_strip_last_eol("", "");
  assert_strip_last_eol("\n", "");
  assert_strip_last_eol("\r", "");
  assert_strip_last_eol("\r\n", "");
  assert_strip_last_eol("\n\r", "\n");
  assert_strip_last_eol("\r\r", "\r");
  assert_strip_last_eol("\n\n", "\n");
  assert_strip_last_eol("yo geeps", "yo geeps");
  assert_strip_last_eol("yo geeps\r\n", "yo geeps");
  assert_strip_last_eol("yo geeps\r\nhow you doin?\r\n", "yo geeps\r\nhow you doin?");
  assert_strip_last_eol("yo geeps\r\nhow you doin?", "yo geeps\r\nhow you doin?");
}
END_TEST

void assert_ends_with_newline(char *input, int expected) {
  stralloc sa = {0}; stralloc_copys(&sa, input);

  int actual = ends_with_newline(&sa);

  ck_assert_int_eq(actual, expected);
}

START_TEST (test_ends_with_newline)
{
  // annoying to test, currently don't believe I have this bug:
  // assert_ends_with_newline(NULL, 0);
  assert_ends_with_newline("", 0);
  assert_ends_with_newline("123", 0);
  assert_ends_with_newline("123\n", 1);
  assert_ends_with_newline("1\n23\n", 1);
}
END_TEST


void assert_is_last_line_of_response(const char *input, int expected)
{
  stralloc sa = {0}; stralloc_copys(&sa, input);
  int actual = is_last_line_of_response(&sa);
  ck_assert_int_eq(actual, expected); }


START_TEST (test_is_last_line_of_response)
{
  //assert_is_last_line_of_response(NULL, 0);
  assert_is_last_line_of_response("", 0);
  assert_is_last_line_of_response("123", 0);
  assert_is_last_line_of_response("1234", 0);
  assert_is_last_line_of_response("123 this is a final line", 1);
  assert_is_last_line_of_response("123-this is NOT a final line", 0);
  assert_is_last_line_of_response("777-is not\r\n", 0);
  assert_is_last_line_of_response("777 is\r\n", 1);
  

  // two surprises, but maybe fine for this function's job:
  // - "\r\n" can be un-present and it's fine
  // - it can have nothing after the space and it's fine
  assert_is_last_line_of_response("123 ", 1);
  assert_is_last_line_of_response("123\n", 0);
}
END_TEST

void assert_parse_client_request(const char *request, const char *verb, const char *arg)
{
  stralloc sa_request = {0}; stralloc_copys(&sa_request, request);
  stralloc sa_request_copy = {0}; stralloc_copy(&sa_request_copy, &sa_request);
  stralloc sa_verb = {0};
  stralloc sa_arg = {0};

  parse_client_request(&sa_verb, &sa_arg, &sa_request);

  ck_assert_int_eq(sa_request_copy.len, sa_request.len);
  stralloc_0(&sa_verb);
  ck_assert_str_eq(sa_verb.s, verb);
  stralloc_0(&sa_arg);
  ck_assert_str_eq(sa_arg.s, arg);
}

START_TEST (test_parse_client_request)
{
  //assert_parse_client_request(NULL, "", "");
  assert_parse_client_request("", "", "");
  assert_parse_client_request("MAIL FROM:<schmonz@schmonz.com>\r\n", "MAIL", "FROM:<schmonz@schmonz.com>");
  assert_parse_client_request("RCPT TO:<geepawhill@geepawhill.org>\r\n", "RCPT", "TO:<geepawhill@geepawhill.org>");
  assert_parse_client_request("GENIUSPROGRAMMER\r\n", "GENIUSPROGRAMMER", "");
  assert_parse_client_request(" NEATO\r\n", "", "NEATO");
  assert_parse_client_request("SWELL \r\n", "SWELL", "");
  assert_parse_client_request(" \r\n", "", "");
  assert_parse_client_request("   \r\n", "", "  ");
  assert_parse_client_request("SUPER WEIRD STUFF\r\n", "SUPER", "WEIRD STUFF");
  assert_parse_client_request("R WEIRD STUFF\r\n", "R", "WEIRD STUFF");
  assert_parse_client_request("MAIL FROM:<schmonz@schmonz.com>\r\nRCPT TO:<geepawhill@geepawhill.org>\r\n", "MAIL", "FROM:<schmonz@schmonz.com>\r\nRCPT TO:<geepawhill@geepawhill.org>");
}
END_TEST

static void assert_get_one_response(const char *input, const char *expected_result, const char *expected_remaining, int expected_return) {
  stralloc actual_one = {0}, actual_many = {0};
  int return_value;
  copys(&actual_many,input);

  return_value = get_one_response(&actual_one,&actual_many);

  ck_assert_int_eq(return_value, expected_return);

  stralloc_0(&actual_one);
  ck_assert_str_eq(actual_one.s, expected_result);

  stralloc_0(&actual_many);
  ck_assert_str_eq(actual_many.s, expected_remaining);
}

START_TEST (test_get_one_response)
{
  assert_get_one_response("777 oneline\r\n", "777 oneline\r\n", "", 1);
  assert_get_one_response("777 separate\r\n888 responses\r\n", "777 separate\r\n", "888 responses\r\n", 1);
  assert_get_one_response("777-two\r\n777 lines\r\n888 three\r\n", "777-two\r\n777 lines\r\n", "888 three\r\n", 1);
  assert_get_one_response("777-two\r\n777 lines\r\n888 three\r\n999 four\r\n", "777-two\r\n777 lines\r\n", "888 three\r\n999 four\r\n", 1);
  assert_get_one_response("777-two\r\n", "", "777-two\r\n", 0);
}
END_TEST