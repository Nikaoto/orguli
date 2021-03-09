#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define LINEBUF_SIZE 4096
#define URLBUF_SIZE  1024
#define NAMEBUF_SIZE 1024

char *help_string =
  "usage: orguli [OPTION] [doc.md] [style.css] > output.html\n"
  "\n"
  "Where OPTION is one of:\n"
  "-s, --single-file: outputs <head> and <meta> tags\n"
  "-h, --help:        displays this help string\n";

enum {
  H1     = 1 << 1,
  H2     = 1 << 2,
  H3     = 1 << 3,
  H4     = 1 << 4,
  H5     = 1 << 5,
  PRE    = 1 << 6,
  CODE   = 1 << 7,
  EM     = 1 << 8,
  STRONG = 1 << 9,
  STRIKE = 1 << 10,
  LIST   = 1 << 11,
  QUOTE  = 1 << 12,
  IND    = 1 << 13, /* on when inside indentation */
};

int prev_line_header = 0;
int cur_line_empty = 0, prev_line_empty = 0; /* for indented code blocks */
int indentation = 0; /* for indented fenced code blocks */
int nesting = 0; /* for nested list items */

int write_text(FILE *fp, char *text, int flags);
int write_img(FILE *fp, char **p, int flags, int link);

/* return 1 if str ends with pat, 0 otherwise */
int strend(char *str, char *pat) {
  size_t sl = 0, pl = strlen(pat);
  while (str[sl]) {
    sl++;
  }
  if (sl < pl) return 0;
  return !strncmp(str + sl - pl, pat, pl);
}

/* return 1 if str starts with pat, 0 otherwise */
int strstart(char *str, char *pat) {
  if (!*str || !*pat) { return 0; }
  while (str && pat && *str == *pat) {
    str++;
    pat++;
  }
  if (!*str && !*pat) { return 1; }
  if (!*str) { return 0; }
  if (!*pat) { return 1; }
  return 0;
}

char* copy_until(char *dst, char *src, char *chars) {
  while (*src && !strchr(chars, *src)) {
    if (*src == '\\') { *dst++ = *src++; }
    *dst++ = *src++;
  }
  *dst = '\0';
  return src;
}

int consume(char **p, char *expect) {
  char *q = *p;
  while (*expect) {
    if (*q++ != *expect++) { return 0; }
  }
  *p = q;
  return 1;
}

void write_fp(FILE *out, FILE *in) {
  int chr;
  while ((chr = fgetc(in)) != EOF) { fputc(chr, out); }
}

void write_b64_fp(FILE *out, FILE *in) {
  char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int n;
  do {
    unsigned char b[3] = {0};
    n = fread(b, 1, 3, in);
    if (n == 0) { break; }
    unsigned x = (b[0] << 16) | (b[1] << 8) | b[2];
    fputc(        t[(x >> 18) & 0x3f],       out);
    fputc(        t[(x >> 12) & 0x3f],       out);
    fputc(n > 1 ? t[(x >>  6) & 0x3f] : '=', out);
    fputc(n > 2 ? t[(x >>  0) & 0x3f] : '=', out);
  } while (n == 3);
}

int write_embedded(FILE *fp, char **p, int flags) {
  char name[NAMEBUF_SIZE];
  *p = copy_until(name, *p, "\n :]*");
  FILE *in = fopen(name, "rb");
  if (in) {
    if (strend(name, ".png") || strend(name, ".jpg") || strend(name, ".gif")) {
      fprintf(fp, "<img src=\"data:image;base64, ");
      write_b64_fp(fp, in);
      fprintf(fp, "\"/>\n");
    } else {
      write_fp(fp, in);
    }
    fclose(in);
    return flags;
  }
  fputc('@', fp);
  return write_text(fp, name, flags);
}

/* parses [text](url) into <a> tag */
int write_link(FILE *fp, char **p, int flags) {
  char text[NAMEBUF_SIZE], url[URLBUF_SIZE];

  /* nested link [![imgname](src)](url) into <a><img></a>*/
  if (consume(p, "![")) {
    char *tmp = *p;
    while (*tmp != '\0' && *tmp != ')') {
      tmp++;
    }

    consume(&tmp, ")](");
    tmp = copy_until(url, tmp, ")");
    fprintf(fp, "<a href=\"%s\">", url);
    flags = write_img(fp, p, flags, 0);
    fprintf(fp, "</a>");
    *p = tmp;
    consume(p, ")");
    return flags;
  }

  /* unnested link */
  *p = copy_until(text, *p, "]");
  if (consume(p, "](")) {
    *p = copy_until(url, *p, ")");
    consume(p, ")");
    fprintf(fp, "<a href=\"%s\">", url);
    flags = write_text(fp, text, flags);
    fprintf(fp, "</a>");
    return flags;
  }
  fputc('[', fp);
  return write_text(fp, text, flags);
}

/* parses ![text](url) into <img> tag if link is 0
   otherwise, into <a><img></a> */
int write_img(FILE *fp, char **p, int flags, int link) {
  char text[NAMEBUF_SIZE], url[URLBUF_SIZE];
  *p = copy_until(text, *p, "]");
  if (consume(p, "](")) {
    *p = copy_until(url, *p, ")");
    consume(p, ")");

    if (link) fprintf(fp, "<a href=\"%s\">", url);
    fprintf(fp, "<img alt=\"%s\" src=\"%s\" />", text, url);
    if (link) fprintf(fp, "</a>");
    return flags;
  } else {
    fputc('[', fp);
    return write_text(fp, text, flags);
  }
}

/* parses <inline_url> into <a> tag */
int write_inline_link(FILE *fp, char **p, int flags) {
  char url[URLBUF_SIZE];
  *p = copy_until(url, *p, ">");
  if (consume(p, ">")) {
    fprintf(fp, "<a href=\"http%s\">http%s</a>", url, url);
    return flags;
  } else {
    fprintf(fp, "<http");
    return write_text(fp, url, flags);
  }
}

/* parses http(s):// links automatically */
int write_auto_link(FILE *fp, char **p, int flags) {
  char url[URLBUF_SIZE];
  *p = copy_until(url, *p, " \t\n");
  if (strstart(url, "://") || strstart(url, "s://")) {
    fprintf(fp, "<a href=\"http%s\">http%s</a>", url, url);
    return flags;
  }
  fprintf(fp, "http");
  return write_text(fp, url, flags);
}

/* writes open/close tag and toggles flag f */
int edge(FILE *fp, int flags, int f, char *tag) {
  if (flags & f) {
    fprintf(fp, "</%s>", tag);
    return flags & ~f;
  }
  fprintf(fp, "<%s>", tag);
  return flags | f;
}

/* writes open/close tag1 and nested tag2 and toggles flag f */
int edge2(FILE *fp, int flags, int f, char *tag1, char *tag2) {
  if (flags & f) {
    fprintf(fp, "</%s></%s>", tag2, tag1);
    return flags & ~f;
  }
  fprintf(fp, "<%s><%s>", tag1, tag2);
  return flags | f;
}

/* autocloses tag and drops flag f */
int drop(FILE *fp, int flags, int f, char *tag) {
  if (tag && (flags & f))
    fprintf(fp, "</%s>", tag);
  return flags & ~f;
}

int drop_inlines(FILE *fp, int flags) {
  return drop(fp, flags, EM, "em") &
         drop(fp, flags, STRONG, "strong") &
         drop(fp, flags, STRIKE, "strike") &
         drop(fp, flags, IND, NULL);
}

char *skip_indentation(char *text, int space_count) {
  while (isspace(*text) && space_count-- > 0)
    text++;
  return text;
}

int write_text(FILE *fp, char *text, int flags) {
  for (char *p = text;; p++) {
top:
    if (~flags & PRE) {
      if (consume(&p,    "`")) {
        flags = edge(fp, flags, CODE, "code");
        goto top;
      }
      if (~flags & CODE) {
        if (consume(&p,  "**")) {
          if (isspace(*p) && isspace(p[-3]))
            fprintf(fp, "**");
          else
            flags = edge(fp, flags, STRONG, "strong");
          goto top;
        }
        if (consume(&p,  "__")) {
          if (isspace(*p) && isspace(p[-3]))
            fprintf(fp, "__");
          else
            flags = edge(fp, flags, EM, "em");
          goto top;
        }
        if (consume(&p, "~~")) {
          if (isspace(*p) && isspace(p[-3]))
            fprintf(fp, "~~");
          else
            flags = edge(fp, flags, STRIKE, "strike");
          goto top;
        }
        if (consume(&p,  "*")) {
          if (isspace(*p) && isspace(p[-2]))
            fprintf(fp, "*");
          else
            flags = edge(fp, flags, EM, "em");
          goto top;
        }
        if (consume(&p,  "_")) {
          if (isspace(*p) && isspace(p[-2]))
            fprintf(fp, "_");
          else
            flags = edge(fp, flags, STRONG, "strong");
          goto top;
        }
        if (consume(&p,  "@")) { flags = write_embedded(fp, &p, flags); goto top; }
        if (consume(&p,  "[")) { flags = write_link(fp, &p, flags); goto top; }
        if (consume(&p, "![")) { flags = write_img(fp, &p, flags, 1); goto top; }
        if (consume(&p, "<http")) { flags = write_inline_link(fp, &p, flags); goto top; }
        if (consume(&p, "http")) { flags = write_auto_link(fp, &p, flags); goto top; }
      }
    }

    if (*p == '\\' || flags & CODE) {
      if (*p == '\\') { p++; }
      switch (*p) {
        case '<' : fprintf(fp, "&lt;");   break;
        case '>' : fprintf(fp, "&gt;");   break;
        case '&' : fprintf(fp, "&amp;");  break;
        case '"' : fprintf(fp, "&quot;"); break;
        case '\'': fprintf(fp, "&apos;"); break;
        case '\0': fputc('\n', fp);       break;
        default  : fputc(*p, fp);         break;
      }
    } else {
      if (*p == '\0')
        return flags;
      fputc(*p, fp);
    }

    if (*p == '\0' || *p == '\n')
      return flags;
  }
}

int process_line(FILE *fp, char *line, char *nextline, int flags) {
  /* empty line */
  if (consume(&line, "\n")) { cur_line_empty = 1; }

  /* header underline, skip line */
  if (consume(&line, "====")) { return flags; }
  if (consume(&line, "----")) { return flags; }

  /* determine if fenced code block is also indented.
    set IND flag and indentation var */
  char *ind_start = strstr(line, "```");
  if (ind_start) {
    int space_count = 0;
    while (isspace(*line)) {
      line++;
      space_count++;
    }

    if (line == ind_start) {
      if (flags & IND) {
        flags &= ~IND;
        indentation = 0;
      } else {
        flags |= IND;
        indentation = space_count;
      }
    }
  }

  /* fenced code block */
  if (consume(&line, "```")) { return edge2(fp, flags, PRE | CODE, "pre", "code"); }
  if (flags & (PRE|IND)) { line = skip_indentation(line, indentation); }
  if (flags & PRE) { return write_text(fp, line, flags); }

  /* hr */
  if (consume (&line, "---")) {
    fprintf(fp, "<hr>");
    return drop_inlines(fp, flags);
  }

  /* hr thick */
  if (consume (&line, "===")) {
    fprintf(fp, "<hr class=\"thick\">");
    return drop_inlines(fp, flags);
  }

  /* skip whitespace and count blanks */
  int spaces = 0;
  while (isspace(*line)) {
    if (isblank(*line)) { spaces++; }
    line++;
  }

  /* quote */
  if (consume(&line, ">")) {
    if (~flags & QUOTE) { flags = edge(fp, flags, QUOTE, "blockquote"); }
    while (isspace(*line)) { line++; }
  } else if (flags & QUOTE && !*line) {
    flags = edge(fp, flags, QUOTE, "blockquote");
  }

  /* list */
  if (consume(&line, "* ") || consume (&line, "- ") || consume (&line, "+ ")) {
    flags = drop_inlines(fp, flags);
    if (flags & LIST) {
      /* nesting */
      if (spaces > nesting) {
        fprintf(fp, "<ul>");
      } else if (spaces < nesting) {
        fprintf(fp, "</ul>");
      }
    } else {
      flags = edge(fp, flags, LIST, "ul");
    }
    nesting = spaces;
    fprintf(fp, "<li>");
  } else if (flags & LIST && !*line) {
    if (nesting) { fprintf(fp, "</ul>"); nesting = 0; }
    flags = edge(fp, flags, LIST, "ul");
    flags = drop_inlines(fp, flags);
  }

  /* indented code block */
  /* TODO: check if spaces > nesting bugs */
  if (spaces > nesting && nextline[0] == '\n' && prev_line_empty) {
    flags = edge2(fp, flags, PRE | CODE, "pre", "code");
    flags = write_text(fp, line, flags);
    return edge2(fp, flags, PRE | CODE, "pre", "code");
  }

  /* new paragraph */
  if (*line == '\0' || prev_line_header) {
    flags = drop_inlines(fp, flags);
    fprintf(fp, "<p>");
  }
  prev_line_header = 0;

  /* header */
  if (strstart(nextline, "====")) { prev_line_header = 1; flags = edge(fp, flags, H1, "h1"); }
  if (strstart(nextline, "----")) { prev_line_header = 1; flags = edge(fp, flags, H2, "h2"); }
  if (consume(&line,     "# ")) { prev_line_header = 1; flags = edge(fp, flags, H1, "h1"); }
  if (consume(&line,    "## ")) { prev_line_header = 1; flags = edge(fp, flags, H2, "h2"); }
  if (consume(&line,   "### ")) { prev_line_header = 1; flags = edge(fp, flags, H3, "h3"); }
  if (consume(&line,  "#### ")) { prev_line_header = 1; flags = edge(fp, flags, H4, "h4"); }
  if (consume(&line, "##### ")) { prev_line_header = 1; flags = edge(fp, flags, H5, "h5"); }

  /* write text */
  flags = write_text(fp, line, flags);

  /* finish header */
  if (flags & H1) { flags = drop_inlines(fp, edge(fp, flags, H1, "h1")); }
  if (flags & H2) { flags = drop_inlines(fp, edge(fp, flags, H2, "h2")); }
  if (flags & H3) { flags = drop_inlines(fp, edge(fp, flags, H3, "h3")); }
  if (flags & H4) { flags = drop_inlines(fp, edge(fp, flags, H4, "h4")); }
  if (flags & H5) { flags = drop_inlines(fp, edge(fp, flags, H5, "h5")); }

  return flags;
}

int main(int argc, char **argv) {
  FILE *css = NULL;
  FILE *in = stdin;

  int single_file = 0;

  for (int i = 1; i < argc; i++) {
    if (!strncmp(argv[i], "-h", 2) || !strncmp(argv[i], "--help", 6)) {
      fprintf(stderr, help_string);
      return 1;
    } else if (!strncmp(argv[i], "-s", 2) || !strncmp(argv[i], "--single-file", 13)) {
      single_file = 1;
    } else if (strend(argv[i], ".css")) {
      css = fopen(argv[i], "rb");
      if (!css) { fprintf(stderr, "error: failed to open .css file\n"); return 1; }
    } else {
      in = fopen(argv[i], "rb");
      if (!in) { fprintf(stderr, "error: failed to open input file\n"); return 1; }
    }
  }

  /* head */
  if (single_file) {
    fprintf(stdout, "<!DOCTYPE html><html><head><meta charset=\"utf-8\">");
  }

  /* style */
  fprintf(stdout, "<style>\n");
  if (css) {
    write_fp(stdout, css);
  } else {
    fprintf(stdout,
      "body{margin:60px auto;max-width:750px;line-height:1.6;"
      "font-family:\"Open Sans\",Arial;color:#444;padding:0 10px;}"
      "h1,h2,h3,h4{line-height:1.2;padding-top: 14px;}");
  }
  fprintf(stdout, "</style>");

  if (single_file) {
    fprintf(stdout, "</head><body class=\"markdown-body\">\n");
  } else {
    fprintf(stdout, "<div class=\"markdown-body\">\n");
  }

  char buf1[LINEBUF_SIZE], buf2[LINEBUF_SIZE];
  char *cur = buf1, *next = buf2, *tmp;
  int flags = 0;

  /* first line */
  if (!fgets(cur, LINEBUF_SIZE, in)) {
    return 1;
  }

  while (fgets(next, LINEBUF_SIZE, in)) {
    flags = process_line(stdout, cur, next, flags);
    prev_line_empty = cur_line_empty;
    cur_line_empty = 0;

    /* swap buffers */
    tmp = cur;
    cur = next;
    next = tmp;
  }

  /* last line */
  flags = process_line(stdout, cur, "\0", flags);

  if (single_file) {
    fprintf(stdout, "</body></html>\n");
  } else {
    fprintf(stdout, "</div>\n");
  }

  return 0;
}
