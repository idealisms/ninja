// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lexer.h"

#include <assert.h>
#include <stdio.h>

#include "eval_env.h"

/*!max:re2c */

void Fatal(const char* msg) {
  printf("FATAL: %s\n", msg);
  assert(0);
}

bool Lexer::Error(const string& message, string* err) {
  // Compute line/column.
  int line = 1;
  const char* context = input_.str_;
  for (const char* p = input_.str_; p < last_token_; ++p) {
    if (*p == '\n') {
      ++line;
      context = p + 1;
    }
  }
  int col = last_token_ ? last_token_ - context : 0;

  char buf[1024];
  snprintf(buf, sizeof(buf), "%s:%d: ", filename_.AsString().c_str(), line);
  *err = buf;
  *err += message + "\n";

  // Add some context to the message.
  if (col > 0) {
    int len;
    for (len = 0; len < 50; ++len) {
      if (context[len] == 0 || context[len] == '\n')
        break;
    }
    *err += string(context, len);
    *err += "\n";
    *err += string(col, ' ');
    *err += "^\n";
  }

  return false;
}

Lexer::Lexer(const char* input) {
  Start("input", input);
}

void Lexer::Start(StringPiece filename, StringPiece input) {
  filename_ = filename;
  input_ = input;
  ofs_ = input_.str_;
  last_token_ = NULL;
}

const char* Lexer::TokenName(Token t) {
  switch (t) {
  case ERROR:    return "lexing error";
  case BUILD:    return "'build'";
  case RULE:     return "'rule'";
  case DEFAULT:  return "'default'";
  case IDENT:    return "identifier";
  case EQUALS:   return "'='";
  case TEOF:     return "eof";
  case INDENT:   return "indent";
  case COLON:    return "':'";
  case PIPE:     return "'|'";
  case PIPE2:    return "'||'";
  case NEWLINE:  return "newline";
  case INCLUDE:  return "'include'";
  case SUBNINJA: return "'subninja'";
  }
  return NULL;
}

void Lexer::UnreadToken() {
  ofs_ = last_token_;
}

Lexer::Token Lexer::ReadToken() {
  const char* p = ofs_;
  const char* q;
  const char* start;
  Lexer::Token token;
  for (;;) {
    start = p;
    char yych;
    /*!re2c
    re2c:define:YYCTYPE = "const char";
    re2c:yych:emit = 0;
    re2c:define:YYCURSOR = p;
    re2c:define:YYMARKER = q;
    re2c:yyfill:enable = 0;

    simple_varname = [a-zA-Z0-9_]+;
    varname = [a-zA-Z0-9_.]+;

    "#".*"\n"  { continue; }
    [\n]       { token = NEWLINE;  break; }
    [ ]+       { token = INDENT;   break; }
    "build"    { token = BUILD;    break; }
    "rule"     { token = RULE;     break; }
    "default"  { token = DEFAULT;  break; }
    "="        { token = EQUALS;   break; }
    ":"        { token = COLON;    break; }
    "||"       { token = PIPE2;    break; }
    "|"        { token = PIPE;     break; }
    "include"  { token = INCLUDE;  break; }
    "subninja" { token = SUBNINJA; break; }
    varname    { token = IDENT;    break; }
    "\000"     { token = TEOF;     break; }
    [^]        { token = ERROR;    break; }
    */
  }

  last_token_ = start;
  ofs_ = p;
  if (token != NEWLINE && token != TEOF)
    EatWhitespace();
  return token;
}

bool Lexer::PeekToken(Token token) {
  Token t = ReadToken();
  if (t == token)
    return true;
  UnreadToken();
  return false;
}

void Lexer::EatWhitespace() {
  const char* p = ofs_;
  for (;;) {
    ofs_ = p;
    char yych;
    /*!re2c
    [ ]+  { continue; }
    "$\n" { continue; }
    [^]   { break; }
    */
  }
}

bool Lexer::ReadIdent(string* out) {
  const char* p = ofs_;
  for (;;) {
    const char* start = p;
    char yych;
    /*!re2c
    varname {
      out->assign(start, p - start);
      break;
    }
    [^] { return false; }
    */
  }
  ofs_ = p;
  EatWhitespace();
  return true;
}

bool Lexer::ReadEvalString(EvalString* eval, bool path, string* err) {
  const char* p = ofs_;
  const char* q;
  const char* start;
  for (;;) {
    start = p;
    char yych;
    /*!re2c
    re2c:yych:emit = 0;
    re2c:yyfill:enable = 0;

    [^$ :\n|\000]+ {
      eval->Add(EvalString::RAW, StringPiece(start, p - start));
      continue;
    }
    [ :|\n] {
      if (path) {
        p = start;
        break;
      } else {
        if (*start == '\n')
          break;
        eval->Add(EvalString::RAW, StringPiece(start, 1));
        continue;
      }
    }
    "$$" {
      eval->Add(EvalString::RAW, StringPiece("$", 1));
      continue;
    }
    "$ " {
      eval->Add(EvalString::RAW, StringPiece(" ", 1));
      continue;
    }
    "$\n"[ ]* {
      continue;
    }
    "${"varname"}" {
      eval->Add(EvalString::SPECIAL, StringPiece(start + 2, p - start - 3));
      continue;
    }
    "$"simple_varname {
      eval->Add(EvalString::SPECIAL, StringPiece(start + 1, p - start - 1));
      continue;
    }
    "\000" {
      last_token_ = start;
      return Error("unexpected EOF", err);
    }
    [^] {
      last_token_ = start;
      return Error("lexing error", err);
    }
    */
  }
  last_token_ = start;
  ofs_ = p;
  if (path)
    EatWhitespace();
  // Non-path strings end in newlines, so there's no whitespace to eat.
  return true;
}
