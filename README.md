# orguli
orguli is a small tool that takes a markdown file with an optional style sheet
and outputs html. It does half of what fully-fledged markdown parsers do with a
tenth of the size (single ~500loc C file).

## Usage
For single-file documents:
```bash
$ orguli -s README.md style.css > README.html
```

For part of a larger document (to concatenate/process/pipe content):
```bash
#! /bin/bash
# Processes and converts all md files to html
for file in *.md
do
    htmlfile=${file%.md}.html
    cat header.html >> $htmlfile
    process-file $file | orguli >> $htmlfile
    cat footer.html >> $htmlfile
done
```

To pipe content to orguli and specify a stylesheet at the same time, specify
`/dev/stdin` as the input file:
```
$ cat README.md | orguli /dev/stdin style.css | sed 's/http:/https:/g' > out.html
```

I use orguli to render README files in my [private git repositories](https://git.nikaoto.com)
by passing orguli to cgit.

## Install
Run `./build.sh` and then `sudo install -m755 orguli /usr/local/bin/.`

## Why?
- No parser does what I want. Most of them have weird ways of behaving, which
  are not documented and only defined in code.
- Rules too complicated. Parsing malformed Markdown is
  [surprizingly complicated](https://spec.commonmark.org/current/)
  and most people don't write complicated things with it, especially when it
  comes to README.md files.
- Extending the other tools is hard compared to a single C99 file. I've found
  that it's faster to just extend orguli than learn the quirks of some large
  parser though trial and error. orguli has it's quirks as well, but they're
  easier to learn and avoid.
- Other parsers are meant for untrusted use, thus adding even more code and
  complications to parsing. orguli has no security at all and is only to be used
  by trusted users. When I just want a simple `md2html` binary for my website, I
  don't care about XSS attacks. I can probably trust myself not to pwn me.

## Features
orguli supports most features defined in
[CommonMark](https://spec.commonmark.org/current/) and
[the extended syntax](https://www.markdownguide.org/extended-syntax).
I chose the parts which most people know and use, mostly in their READMEs.

### Missing features
Due to the design, anything that can't be determined with a single-line
lookahead will forever be unsupported. This means orguli can never support
nesting deeper than 2 levels.

#### Missing features I plan to support:
1. heading ids
2. emoji
3. task lists

#### Missing features which can be added with the help of preprocessors:
1. markdown tables (html `<table>`s already work normally)
2. definition lists
3. footnotes
4. reflinks

These features can be implemented with the help of simple preprocessors that
convert unsupported markup into markup that orguli understands or just html
which orguli will skip over.

For example, given the input file `input.md`:

```
This is [my reflink][my-reflink].

[my-reflink]: https://example.com
```
we can build a pipeline
```bash
$ process-reflinks input.md | orguli > output.html
```
where `process-reflinks` reads `input.md` completely and outputs
```
This is [my reflink](https://example.com)
```
or
```
This is <a href="https://example.com">my reflink</a>
```
orguli would handle both variants easily, converting the first one and skipping
the html in the second.

Writing and maintaining small preprocessors like these for whichever feature one
wants is easier than modifying programs even as small as orguli.

### Extra features
The only extra feature not present in markdown is the `@filename` specifier,
which embeds images and text directly. This is extremely useful for single-file
documents.

## Changes
orguli is based on rxi's [doq](https://github.com/rxi/doq), which now seems to
be abandoned. Here is a list of the changes I introduced.

### Added
- support for nested lists 2 levels deep
- support for numbered lists and mixed nesting
- support for list items starting with `- ` and `+ `
- support for auto-detecting and linking `http(s)://`
- support for `<inline links like this>`
- support for `![inline images](image_link)]`
- support for `[![images nested in links](img_src)](link_href)`
- support for nested fenced code blocks
- support for `<pre><code> code blocks </code></pre>` with edge2
- single line lookahead (to support h1 and h2 with `====` and h2 `----`)
- `-s|--single-file` option, which outputs `<head>` and others. When omitted,
  output without `<head>`
- `-h|--help` option
- support for autoclosing tags; drop inline flags at edges of headers, lists,
  and blocks
- more efficient string functions in place of strstr()
- support for `**`(strong) and `__`(em).
- fix for false em, strike, and strong detection on separate chars:<br>
  `2 * 10` will no longer turn into `2 <em> 10`, but `2 *10` will correctly turn
  into `2 <em>10`
- support for `<hr>` (`---` and `___` for `<hr>`, `===` for `<hr class="thick">`)
- support for inline html, including `<br>` tags
- comments where necessary
- `.markdown-body` class to main `<div>` or `<body>`
- support for h4 and h5
- support for indented code blocks

    this will be turned into code (not on github)

Just like
```
this fenced code block
```

### Removed
- dependency on `stdbool.h`
- static scope specifiers from all functions
- automatic escape on all text; If you want something escaped, use
  backslashes.<br>
  For example, `\<` turns into `&lt;`

## License
This library is free software; you can redistribute is and/or modify it under
the terms of the MIT license. See [LICENSE](./LICENSE) for details.
