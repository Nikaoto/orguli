# orguli
orguli is a small tool that takes a markdown file with an optional style sheet
and outputs html. It does half of what fully-fledged markdown parsers do with a
tenth of the size (single ~500loc C file).

## Usage
For single-file documents:
```bash
orguli -s README.md style.css > readme.html
```

For part of a larger document (to concatenate/process/pipe content):
```bash
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
`/dev/stdin` as the input markdown file:
```
# Use /dev/stdin as the input file
cat README.md | orguli /dev/stdin style.css | sed 's/http:/https:/g' > out.html
```

I use orguli to render README files in my [private git repositories](https://git.nikaoto.com)
by passing orguli to cgit.

## Install
Run `./build.sh` and then `sudo install -m755 orguli /usr/local/bin/.`

## Why?
- No parser does what I want. Most of them have weird ways of behaving, which
  are not documented and only defined in code.
- Too many unnecessary features. The Markdown language is
  [surprizingly large](https://spec.commonmark.org/current/) and most people
  don't even use half of it, especially when it comes to README.md files.
- Extending the other tools is hard. A single C99 file is easier to modify and
  I've found that it's faster to just extend orguli than learn the quirks of
  some large parser though trial and error. orguli also has it's quirks as well,
  but they're easier to learn and avoid.
- Other parsers are meant for untrusted use. orguli has no security at all and
  is only to be used by trusted users. This might seem like a disadvantage, but
  lack of security allows for less and easier to understand and modify code.
  When I just want a simple `md2html` binary for my website, I don't care about
  XSS attacks. I can probably trust myself not to pwn me.

## Limitations
orguli complies with the *right* half of markdown, meaning the parts which most
people know and use, mostly in their READMEs.

Due to the design, anything that can't be determined with a single-line
lookahead will forever be unsupported. This nesting levels higher than 2,
footnotes, and reflinks. However, the latter two can be implemented with simple
preprocessors that convert them into html or markdown that orguli supports.
For example, with the input file `input.md`:
```
This is [my reflink][my-reflink].

[my-reflink]: https://example.com
```
we can build a pipeline
```
process-reflinks input.md | orguli > output.html
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

The only extra feature not present in markdown is the `@filename` specifier,
which embeds images and text directly. This is extremely useful for single-file
documents.

## Changes
orguli is a fork of rxi's [doq](https://github.com/rxi/doq), which now seems to
be abandoned. Here is a list of the changes I introduced.

### Added
- support for list items starting with `- ` and `+ `
- support for auto-detecting and linking `http(s)://`
- support for `<inline links like this>`
- support for `![inline images](image_link)]`
- support for `[![images nested in links](img_src)](link_href)`
- support for nested fenced code blocks
- support for `<pre><code> code blocks </code></pre>` with edge2
- support fot nested lists (only 2 levels deep!)
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
- support for `<hr>` (`---` for `<hr>`, `===` for `<hr class="thick">`)
- support for inline html, including `<br>` tags
- comments where necessary
- `.markdown-body` class to main `<div>` or `<body>`
- `style.css` file
- support for h4 and h5
- support for indented code blocks

    this will be turned into code

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
