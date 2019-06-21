/*
 * Copyright (c) 2004-2013 Sergey Lyubka <valenok@gmail.com>
 * Copyright (c) 2013 Cesanta Software Limited
 * All rights reserved
 *
 * This library is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this library under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this library under a commercial
 * license, as set out in <http://cesanta.com/products.html>.
 */

/*
 * This is a regular expression library that implements a subset of Perl RE.
 * Please refer to README.md for a detailed reference.
 */

#ifndef CS_SLRE_SLRE_H_
#define CS_SLRE_SLRE_H_

#ifdef __cplusplus
extern "C" {
#endif

struct slre_cap {
  const char *ptr;
  int len;
};

#define MAX_BRANCHES 100
#define MAX_BRACKETS 100

struct slre_bracket_pair {
  const char *ptr;  /* Points to the first char after '(' in regex  */
  int len;          /* Length of the text between '(' and ')'       */
  int branches;     /* Index in the branches array for this pair    */
  int num_branches; /* Number of '|' in this bracket pair           */
};

struct slre_branch {
  int bracket_index;    /* index for 'struct slre_bracket_pair brackets' */
                        /* array defined below                      */
  const char *schlong;  /* points to the '|' character in the regex */
};

struct slre_regex_info {
  /*
   * Describes all bracket pairs in the regular expression.
   * First entry is always present, and grabs the whole regex.
   */
  struct slre_bracket_pair brackets[MAX_BRACKETS];
  int num_brackets;

  /*
   * Describes alternations ('|' operators) in the regular expression.
   * Each branch falls into a specific branch pair.
   */
  struct slre_branch branches[MAX_BRANCHES];
  int num_branches;

  /* Array of captures provided by the user */
  struct slre_cap *caps;
  int num_caps;

  /* E.g. SLRE_IGNORE_CASE. See enum below */
  int flags;
};

int slre_compile(const char *regexp, int re_len, int flags,
                 struct slre_regex_info *info);


int slre_match(const char *regexp, const char *s, int s_len,
               struct slre_cap *caps, int num_caps, int flags);

int slre_match_reuse(struct slre_regex_info *info, const char *s,
                     int s_len, struct slre_cap *caps, int num_caps);

/* Possible flags for slre_match() */
enum { SLRE_IGNORE_CASE = 1 };


/* slre_match() failure codes */
#define SLRE_NO_MATCH               (-1)
#define SLRE_UNEXPECTED_QUANTIFIER  (-2)
#define SLRE_UNBALANCED_BRACKETS    (-3)
#define SLRE_INTERNAL_ERROR         (-4)
#define SLRE_INVALID_CHARACTER_SET  (-5)
#define SLRE_INVALID_METACHARACTER  (-6)
#define SLRE_CAPS_ARRAY_TOO_SMALL   (-7)
#define SLRE_TOO_MANY_BRANCHES      (-8)
#define SLRE_TOO_MANY_BRACKETS      (-9)

#ifdef __cplusplus
}
#endif

#endif /* CS_SLRE_SLRE_H_ */
